#include "KeyLockStretcher.h"

#include <algorithm>
#include <cmath>

namespace
{
	using RB = RubberBand::RubberBandStretcher;

	// R2, the faster engine, in real time: two decks of four stereo stems on
	// the rig's CPU. Crisp transients with the compound detector reset the
	// phases on each hit, so the drums keep their attack; laminar phase
	// keeps bass, pads and vocals coherent across bins, where independent
	// phase sounds softer and phasier. The standard window: the short one
	// would serve the drums and thin the bass. The two channels of a stem
	// together: a stereo stem keeps its image and its mono sum, which the
	// 3D encoding downstream relies on. No threads of its own on the audio
	// thread.
	constexpr RB::Options options = RB::OptionProcessRealTime | RB::OptionEngineFaster
								  | RB::OptionTransientsCrisp | RB::OptionDetectorCompound
								  | RB::OptionPhaseLaminar | RB::OptionWindowStandard
								  | RB::OptionChannelsTogether | RB::OptionThreadingNever;

	// A stretcher that keeps asking for nothing and giving nothing: give up
	// the block rather than spin on the audio thread.
	constexpr int maxRounds = 256;
}

KeyLockStretcher::KeyLockStretcher (int channels, double fileRate)
	: KeyLockStretcher (channels, fileRate, measureAlignment (fileRate))
{
}

KeyLockStretcher::KeyLockStretcher (int channels, double fileRate, const keylock::Alignment& known)
	: numChannels (channels), sampleRate (fileRate),
	  stretcher ((size_t) fileRate, (size_t) channels, options),
	  alignment (known)
{
	stretcher.setMaxProcessSize ((size_t) chunk);

	inputStore.assign ((size_t) channels, std::vector<float> ((size_t) chunk));
	scratchStore.assign ((size_t) channels, std::vector<float> ((size_t) chunk));
	for (size_t ch = 0; ch < (size_t) channels; ++ch)
	{
		inputs.push_back (inputStore[ch].data());
		scratch.push_back (scratchStore[ch].data());
	}
	offsetPointers.resize ((size_t) channels);
}

void KeyLockStretcher::restart (std::int64_t position, double playbackRate, Input& input)
{
	beginRestart (position, playbackRate);
	while (padding)
		padStep (input);
}

void KeyLockStretcher::beginRestart (std::int64_t position, double playbackRate)
{
	stretcher.reset();
	runs.clear();
	setRate (playbackRate);
	startRate = playbackRate;
	ended = false;
	toDiscard = 0;

	// Rubber Band asks for silence before the input and its start delay
	// thrown away, for the output to line up with the input (exactly at
	// rate 1; startDiscard() moves it for the others). The audio before
	// `position` instead of the silence lines up the same, and the first
	// block is already music, not a fade-in.
	padAt = position - (std::int64_t) stretcher.getPreferredStartPad();
	padTo = position;
	padding = true;
}

bool KeyLockStretcher::prime (Input& input, int budget)
{
	int spent = 0;
	do
	{
		if (padding)
			spent += padStep (input);
		else if (toDiscard > 0)
			spent += discardStep (input);
	} while (! isPrimed() && spent < budget);
	return isPrimed();
}

// One chunk of the audio before the start; the last one sets the start up.
int KeyLockStretcher::padStep (Input& input)
{
	int count = 0;
	if (padAt < padTo)
	{
		count = (int) std::min<std::int64_t> (chunk, padTo - padAt);
		for (auto& channel : inputStore)
			std::fill (channel.begin(), channel.begin() + count, 0.0f);

		// Before the track's start it stays silent.
		const auto skip = (int) std::clamp<std::int64_t> (-padAt, 0, count);
		for (size_t ch = 0; ch < offsetPointers.size(); ++ch)
			offsetPointers[ch] = inputs[ch] + skip;
		if (skip < count)
			input.read (offsetPointers.data(), count - skip, padAt + skip);

		process (count);
		padAt += count;
	}

	if (padAt >= padTo)
	{
		toDiscard = (std::size_t) keylock::startDiscard (stretcher.getStartDelay(), rate, alignment);
		cursor = padTo;
		map.restart (padTo);
		padding = false;
	}
	return count;
}

// One step of throwing the start delay away: take what is there, or feed.
int KeyLockStretcher::discardStep (Input& input)
{
	if (const auto available = stretcher.available(); available > 0)
	{
		const auto take = std::min ({ (std::size_t) available, toDiscard, (std::size_t) chunk });
		stretcher.retrieve (scratch.data(), take);
		runs.consume ((int) take, rate);
		toDiscard -= take;
		return 0;
	}
	const auto before = processed;
	feed (input);
	return (int) (processed - before);
}

void KeyLockStretcher::render (float* const* out, int numSamples, double playbackRate, Input& input)
{
	setRate (playbackRate);
	discardStartDelay (input);

	int done = 0;

	for (int round = 0; done < numSamples; ++round)
	{
		if (round == maxRounds)
		{
			for (int ch = 0; ch < numChannels; ++ch)
				std::fill (out[ch] + done, out[ch] + numSamples, 0.0f);
			break;
		}

		if (const auto available = stretcher.available(); available > 0)
		{
			const auto take = std::min (available, numSamples - done);
			for (size_t ch = 0; ch < offsetPointers.size(); ++ch)
				offsetPointers[ch] = out[ch] + done;
			stretcher.retrieve (offsetPointers.data(), (size_t) take);
			map.heard (runs.consume (take, rate));
			done += take;
		}
		else
		{
			feed (input);
		}
	}
}

void KeyLockStretcher::discardStartDelay (Input& input)
{
	for (int round = 0; toDiscard > 0 && round < maxRounds; ++round)
		discardStep (input);
}

void KeyLockStretcher::feed (Input& input)
{
	const auto required = (int) std::min<std::size_t> (stretcher.getSamplesRequired(), (std::size_t) chunk);
	const auto count = required > 0 ? required : chunk / 4;
	fill (input, count);
	process (count);
}

// What a process() call makes is made at the ratio now set: recorded so
// that it is counted at that rate when it comes out.
void KeyLockStretcher::process (int count)
{
	const auto before = stretcher.available();
	stretcher.process (inputs.data(), (size_t) count, false);
	processed += count;
	runs.produced (stretcher.available() - before, rate);
}

void KeyLockStretcher::setRate (double newRate)
{
	rate = newRate;
	stretcher.setTimeRatio (keylock::timeRatio (newRate));
}

// `count` frames from the cursor on, following loops and repeats as the
// track does; after the track's end, silence.
void KeyLockStretcher::fill (Input& input, int count)
{
	int filled = 0;
	bool stalled = false;

	while (filled < count && ! ended)
	{
		for (size_t ch = 0; ch < offsetPointers.size(); ++ch)
			offsetPointers[ch] = inputs[ch] + filled;

		const auto got = input.read (offsetPointers.data(), count - filled, cursor);
		cursor += got;
		filled += got;
		map.fed (got);

		if (filled == count)
			break;

		// Two stops in a row without a frame between: an empty loop.
		const auto next = input.jumpFrom (cursor);
		if (next < 0 || (got == 0 && stalled))
		{
			ended = true;
			break;
		}

		stalled = got == 0;
		cursor = next;
		map.jumped (next);
	}

	if (filled < count)
	{
		for (int ch = 0; ch < numChannels; ++ch)
			std::fill (inputs[(size_t) ch] + filled, inputs[(size_t) ch] + count, 0.0f);
		map.fed (count - filled);
	}
}

//==============================================================================
namespace
{
	// A single click in silence: where the output's loudest sample is tells
	// where the stretcher really is.
	struct Click : KeyLockStretcher::Input
	{
		explicit Click (std::int64_t position) : at (position) {}

		int read (float* const* dest, int count, std::int64_t position) override
		{
			std::fill (dest[0], dest[0] + count, 0.0f);
			if (at >= position && at < position + count)
				dest[0][at - position] = 1.0f;
			return count;
		}

		std::int64_t jumpFrom (std::int64_t) override { return -1; }

		const std::int64_t at;
	};
}

// Started at `startRate`, after the first block at `rate`: how far the
// reported playhead is from the click when the click is heard, in input
// samples (positive: the playhead is ahead of the ear).
static double playheadError (KeyLockStretcher& stretcher, double startRate, double rate)
{
	constexpr int block = 256;
	const auto at = (std::int64_t) stretcher.getSampleRate() / 2;
	Click click { at };
	stretcher.restart (0, startRate, click);

	std::vector<float> out ((size_t) block);
	float* channels[] = { out.data() };
	float loudest = 0.0f;
	double error = 0.0;

	for (int b = 0; b < (int) (at / block / std::min (startRate, rate)) + 64; ++b)
	{
		const auto r = b == 0 ? startRate : rate;
		const auto before = stretcher.position();
		stretcher.render (channels, block, r, click);

		for (int i = 0; i < block; ++i)
			if (std::abs (out[(size_t) i]) > loudest)
			{
				loudest = std::abs (out[(size_t) i]);
				error = before + i * r - (double) at;
			}
	}
	return error;
}

// One channel tells it for any number: the window and hops are the same.
keylock::Alignment KeyLockStretcher::measureAlignment (double sampleRate)
{
	constexpr double away = 1.25;   // far enough from 1 to measure in samples
	keylock::Alignment alignment;

	KeyLockStretcher probe { 1, sampleRate, alignment };
	alignment.startOffset = playheadError (probe, away, away) / (away - 1.0);

	probe.alignment = alignment;
	alignment.rateLag = playheadError (probe, 1.0, away) / (away - 1.0);
	return alignment;
}
