#include <gtest/gtest.h>

#include "KeyLockStretcher.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
	constexpr double sampleRate = 44100.0;
	constexpr int channels = 2;   // a stereo stem; a deck runs one per stem
	constexpr int block = 512;

	// An endless sine on every channel.
	struct Sine : KeyLockStretcher::Input
	{
		explicit Sine (double hz) : frequency (hz) {}

		int read (float* const* dest, int count, std::int64_t position) override
		{
			for (int i = 0; i < count; ++i)
			{
				const auto v = (float) std::sin (2.0 * M_PI * frequency * (double) (position + i) / sampleRate) * 0.5f;
				for (int ch = 0; ch < channels; ++ch)
					dest[ch][i] = v;
			}
			return count;
		}

		std::int64_t jumpFrom (std::int64_t) override { return -1; }

		double frequency;
	};

	// Silence with a single click at `at`, which ends at `length`; with
	// `loop`, it jumps from loopEnd back to loopStart.
	struct Click : KeyLockStretcher::Input
	{
		int read (float* const* dest, int count, std::int64_t position) override
		{
			const auto stop = loopEnd > 0 && position < loopEnd ? loopEnd : length;
			const auto n = (int) std::clamp<std::int64_t> (stop - position, 0, count);
			for (int i = 0; i < n; ++i)
				for (int ch = 0; ch < channels; ++ch)
					dest[ch][i] = position + i == at ? 1.0f : 0.0f;
			return n;
		}

		std::int64_t jumpFrom (std::int64_t position) override
		{
			return loopEnd > 0 && position == loopEnd ? loopStart : -1;
		}

		std::int64_t at = 0, length = 1 << 20, loopStart = 0, loopEnd = 0;
	};

	// Channel 0 of `numBlocks` blocks at `rate`; positions[i]: the playhead
	// the stretcher reports after block i.
	std::vector<float> render (KeyLockStretcher& stretcher, KeyLockStretcher::Input& input, double rate, int numBlocks,
							   std::vector<double>* positions = nullptr)
	{
		std::vector<std::vector<float>> out ((size_t) channels, std::vector<float> ((size_t) block));
		std::vector<float*> pointers;
		for (auto& c : out)
			pointers.push_back (c.data());

		std::vector<float> result;
		for (int b = 0; b < numBlocks; ++b)
		{
			stretcher.render (pointers.data(), block, rate, input);
			result.insert (result.end(), out[0].begin(), out[0].end());
			if (positions != nullptr)
				positions->push_back (stretcher.position());
		}
		return result;
	}

	// The frequency from the upward zero crossings, interpolated, after `skip`.
	double frequencyOf (const std::vector<float>& signal, size_t skip)
	{
		double first = -1.0, last = -1.0;
		int crossings = 0;
		for (size_t i = skip + 1; i < signal.size(); ++i)
			if (signal[i - 1] < 0.0f && signal[i] >= 0.0f)
			{
				const auto t = (double) (i - 1) + signal[i - 1] / (signal[i - 1] - signal[i]);
				if (first < 0.0)
					first = t;
				last = t;
				++crossings;
			}
		return crossings > 1 ? (crossings - 1) * sampleRate / (last - first) : 0.0;
	}

	// Playback without key lock: read faster, as the resampler does.
	std::vector<float> plainAt (double rate, double hz, size_t length)
	{
		std::vector<float> out (length);
		for (size_t i = 0; i < length; ++i)
			out[i] = (float) std::sin (2.0 * M_PI * hz * (double) i * rate / sampleRate);
		return out;
	}

	size_t loudest (const std::vector<float>& signal)
	{
		size_t best = 0;
		for (size_t i = 0; i < signal.size(); ++i)
			if (std::abs (signal[i]) > std::abs (signal[best]))
				best = i;
		return best;
	}
}

// The point of key lock: 25 % faster, the same note. Without it the note
// rises with the tempo.
TEST (KeyLockStretcher, ATempoChangeKeepsThePitch)
{
	Sine sine { 441.0 };
	KeyLockStretcher stretcher { channels, sampleRate };
	stretcher.restart (0, 1.25, sine);
	const auto locked = render (stretcher, sine, 1.25, 200);
	EXPECT_NEAR (frequencyOf (locked, 22050), 441.0, 441.0 * 0.01);

	EXPECT_NEAR (frequencyOf (plainAt (1.25, 441.0, locked.size()), 0), 441.0 * 1.25, 441.0 * 1.25 * 0.01);
}

TEST (KeyLockStretcher, SlowerKeepsThePitchToo)
{
	Sine sine { 441.0 };
	KeyLockStretcher stretcher { channels, sampleRate };
	stretcher.restart (0, 0.92, sine);
	EXPECT_NEAR (frequencyOf (render (stretcher, sine, 0.92, 200), 22050), 441.0, 441.0 * 0.01);
}

// The latency compensation: a click in the track is heard when the reported
// playhead is on it -- so waveform, grid, sync and the beat clock see what
// is heard, not what the stretcher reads ahead (some 35 ms at 44.1 kHz).
// Within half a millisecond, from half to double speed.
TEST (KeyLockStretcher, TheReportedPlayheadIsWhatIsHeard)
{
	for (const auto rate : { 0.5, 0.8, 0.92, 1.0, 1.04, 1.08, 1.16, 1.25, 1.5, 2.0 })
	{
		Click click;
		click.at = 30000;
		KeyLockStretcher stretcher { channels, sampleRate };
		stretcher.restart (10000, rate, click);
		const auto out = render (stretcher, click, rate, 100);
		const auto heardAt = 10000.0 + (double) loudest (out) * rate;
		EXPECT_NEAR (heardAt, 30000.0, sampleRate / 2000.0) << "rate " << rate;
	}
}

// The tempo fader moved, a sync nudge: the playhead stays on what is heard,
// long after the change too. Within a millisecond.
TEST (KeyLockStretcher, ThePlayheadStaysOnTheEarAfterATempoChange)
{
	for (const auto [startRate, rate] : { std::pair { 1.0, 1.08 }, { 1.08, 1.0 }, { 1.0, 0.92 }, { 0.92, 1.08 }, { 1.0, 1.16 }, { 1.02, 0.98 } })
	{
		Click click;
		click.at = 200000;   // some 4 s in
		KeyLockStretcher stretcher { channels, sampleRate };
		stretcher.restart (10000, startRate, click);

		std::vector<std::vector<float>> out ((size_t) channels, std::vector<float> ((size_t) block));
		std::vector<float*> pointers;
		for (auto& c : out)
			pointers.push_back (c.data());

		float loudest = 0.0f;
		double heardAt = 0.0;
		for (int b = 0; b < 500; ++b)
		{
			const auto r = b < 10 ? startRate : rate;
			const auto before = stretcher.position();
			stretcher.render (pointers.data(), block, r, click);
			for (int i = 0; i < block; ++i)
				if (std::abs (out[0][(size_t) i]) > loudest)
				{
					loudest = std::abs (out[0][(size_t) i]);
					heardAt = before + i * r;
				}
		}
		EXPECT_NEAR (heardAt, 200000.0, sampleRate / 1000.0) << startRate << " -> " << rate;
	}
}

// A restart (seek, loop set, key lock switched on) starts on the spot: no
// silence while the stretcher fills, and the playhead starts where it was put.
TEST (KeyLockStretcher, ARestartPlaysAtOnceFromThere)
{
	Sine sine { 441.0 };
	KeyLockStretcher stretcher { channels, sampleRate };
	stretcher.restart (88200, 1.08, sine);
	EXPECT_DOUBLE_EQ (stretcher.position(), 88200.0);

	std::vector<double> positions;
	const auto out = render (stretcher, sine, 1.08, 4, &positions);
	float firstBlockPeak = 0.0f;
	for (int i = 0; i < block; ++i)
		firstBlockPeak = std::max (firstBlockPeak, std::abs (out[(size_t) i]));
	EXPECT_GT (firstBlockPeak, 0.25f) << "sound in the first block";
	EXPECT_NEAR (positions[0], 88200.0 + block * 1.08, 1e-6);
	EXPECT_NEAR (positions[3], 88200.0 + 4 * block * 1.08, 1e-6);
}

// In a loop the reader wraps ahead of the ear; the playhead wraps when the
// wrap is heard, and stays inside the loop.
TEST (KeyLockStretcher, InALoopThePlayheadStaysInTheLoop)
{
	Click click;
	click.at = -1;
	click.loopStart = 44100;
	click.loopEnd = 44100 + 22050;
	KeyLockStretcher stretcher { channels, sampleRate };
	stretcher.restart (60000, 1.0, click);

	std::vector<double> positions;
	render (stretcher, click, 1.0, 400, &positions);
	for (const auto p : positions)
	{
		EXPECT_GE (p, 44100.0);
		EXPECT_LT (p, 44100.0 + 22050.0);
	}
}

TEST (KeyLockStretcher, TheEndOfTheTrackIsSilence)
{
	Click click;
	click.at = -1;
	click.length = 20000;
	KeyLockStretcher stretcher { channels, sampleRate };
	stretcher.restart (0, 1.0, click);
	render (stretcher, click, 1.0, 100);
	EXPECT_TRUE (stretcher.hasEnded());
	EXPECT_GT (stretcher.position(), 20000.0) << "the playhead runs on past the end; the player stops there";
}
