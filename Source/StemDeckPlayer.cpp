#include "StemDeckPlayer.h"
#include "Remote.h"
#include "GridEdit.h"
#include "Buses.h"

StemDeckPlayer::StemDeckPlayer (juce::AudioFormatManager& fm) : formatManager (fm)
{
	for (int i = 0; i < numStems; ++i)
	{
		stemGain[(size_t) i] = 1.0f;
		stemMuted[(size_t) i] = false;
		stemBuses[(size_t) i] = buses::defaultMask (i);
		stemPeak[(size_t) i] = 0.0f;
	}

	diskThread.addTimeSliceClient (this);
	diskThread.startThread (juce::Thread::Priority::normal);
	bufferThread.startThread (juce::Thread::Priority::normal);
}

StemDeckPlayer::~StemDeckPlayer()
{
	diskThread.removeTimeSliceClient (this);
	diskThread.stopThread (2000);

	{
		const juce::ScopedLock sl (setLock);
		loaded.reset(); // buffered readers must go before bufferThread stops
	}

	bufferThread.stopThread (2000);
}

juce::String StemDeckPlayer::load (const StemSet& set)
{
	auto newSet = std::make_shared<LoadedSet>();

	for (int i = 0; i < numStems; ++i)
	{
		const auto& file = set.files[(size_t) i];
		auto* format = formatManager.findFormatForFileExtension (file.getFileExtension());

		if (format == nullptr)
			return "Unbekanntes Format: " + file.getFileName();

		std::unique_ptr<juce::AudioFormatReader> reader;

		if (std::unique_ptr<juce::MemoryMappedAudioFormatReader> mapped { format->createMemoryMappedReader (file) };
			mapped != nullptr && mapped->mapEntireFile())
		{
			newSet->mapped[(size_t) i] = mapped.get();
			reader = std::move (mapped);
		}
		else if (auto* plain = formatManager.createReaderFor (file))
		{
			// Keep ~10 s buffered ahead; the callback never blocks on it.
			reader = std::make_unique<juce::BufferingAudioReader> (plain, bufferThread, (int) (plain->sampleRate * 10));
		}

		if (reader == nullptr)
			return "Cannot read the file: " + file.getFileName();

		if (i == 0)
			newSet->sampleRate = reader->sampleRate;
		else if (! juce::approximatelyEqual (reader->sampleRate, newSet->sampleRate))
			return "Stems haben unterschiedliche Samplerates: " + set.name;

		newSet->length = juce::jmax (newSet->length, reader->lengthInSamples);
		newSet->readers[(size_t) i] = std::move (reader);
	}

	playing = false;
	scratching = false;
	readPosition = 0;
	loopEnd = 0;
	gridBpm = 0.0;
	loopStart = 0;
	cuePoint = 0.0;
	fileSampleRate = newSet->sampleRate;
	lengthInSamples = newSet->length;

	{
		const juce::ScopedLock sl (setLock);
		std::swap (loaded, newSet);
	}

	// The previous set (now in newSet) is released here, outside the lock.
	resampler.flushBuffers();
	updateResamplingRatio();
	return {};
}

void StemDeckPlayer::play()
{
	if (! isLoaded())
		return;

	if (readPosition.load() >= lengthInSamples.load())
		readPosition = hasLoop() ? loopStart.load() : 0;

	playing = true;
}

void StemDeckPlayer::pause()
{
	playing = false;
}

void StemDeckPlayer::stop()
{
	playing = false;
	readPosition = hasLoop() ? loopStart.load() : 0;
	resampler.flushBuffers();
}

void StemDeckPlayer::setPosition (double seconds)
{
	readPosition = juce::jlimit ((juce::int64) 0, lengthInSamples.load(), (juce::int64) (seconds * fileSampleRate.load()));
	resampler.flushBuffers();
}

double StemDeckPlayer::getPosition() const
{
	return (double) readPosition.load() / fileSampleRate.load();
}

double StemDeckPlayer::getLength() const
{
	return (double) lengthInSamples.load() / fileSampleRate.load();
}

void StemDeckPlayer::setLoop (double requestedStart, double requestedEnd)
{
	// On the beat grid, from wherever it comes: the overview drag, the
	// controller's loop in/out, a restored session (2026-09-30).
	const auto snapped = GridEdit::snappedLoop ({ gridBpm.load(), gridFirstBeat.load() }, requestedStart, requestedEnd);
	const auto startSeconds = snapped.start, endSeconds = snapped.end;
	const auto rate = fileSampleRate.load();
	const auto length = lengthInSamples.load();
	const auto start = juce::jlimit ((juce::int64) 0, length, (juce::int64) (startSeconds * rate));
	const auto end = juce::jlimit ((juce::int64) 0, length, (juce::int64) (endSeconds * rate));

	// Reject empty or reversed regions rather than handing them to the audio thread.
	if (end - start < (juce::int64) (rate * 0.01))
		return;

	loopEnd = 0; // disable while the bounds change
	loopStart = start;
	loopEnd = end;
	setPosition (startSeconds);
}

void StemDeckPlayer::clearLoop()
{
	loopEnd = 0;
}

juce::Range<double> StemDeckPlayer::getLoop() const
{
	const auto rate = fileSampleRate.load();
	return hasLoop() ? juce::Range<double> ((double) loopStart.load() / rate, (double) loopEnd.load() / rate)
					 : juce::Range<double>();
}

void StemDeckPlayer::setSpeed (double ratio)
{
	speed = ratio;
	updateResamplingRatio();
}

void StemDeckPlayer::setStemGain (int stem, float gain)
{
	stemGain[(size_t) stem] = gain;
}

void StemDeckPlayer::setStemMuted (int stem, bool muted)
{
	stemMuted[(size_t) stem] = muted;
}

float StemDeckPlayer::popStemPeak (int stem)
{
	return stemPeak[(size_t) stem].exchange (0.0f);
}

remote::Level StemDeckPlayer::popDeskLevel (int stem)
{
	return deskLevels[(size_t) stem].pop();
}

void StemDeckPlayer::updateResamplingRatio()
{
	resampler.setResamplingRatio (getEffectiveRate() * fileSampleRate.load() / deviceSampleRate.load());
}

void StemDeckPlayer::beginScratch()
{
	scratchTarget = (double) readPosition.load();
	scratching = true;
}

void StemDeckPlayer::scratchBy (double seconds)
{
	const auto target = scratchTarget.load() + seconds * fileSampleRate.load();
	scratchTarget = juce::jlimit (0.0, (double) lengthInSamples.load(), target);
}

//==============================================================================
void StemDeckPlayer::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
	deviceSampleRate = sampleRate;
	updateResamplingRatio();
	resampler.prepareToPlay (samplesPerBlockExpected, sampleRate);

	// Room for one block at the highest scratch speed (plus interpolation margin).
	const auto maxBlock = juce::jmax (samplesPerBlockExpected, 4096);
	scratchBuffer.setSize (numOutputChannels, (int) (maxBlock * maxScratchSpeed * 4) + 8);

	for (int i = 0; i < numStems; ++i)
	{
		gainSmoothers[(size_t) i].reset (sampleRate, 0.02);
		gainSmoothers[(size_t) i].setCurrentAndTargetValue (stemMuted[(size_t) i] ? 0.0f : stemGain[(size_t) i].load());
	}
}

void StemDeckPlayer::releaseResources()
{
	resampler.releaseResources();
}

void StemDeckPlayer::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
	jassert (info.buffer->getNumChannels() >= numOutputChannels);

	// Only ever contended for a pointer copy/swap, so waiting beats a dropout.
	const juce::ScopedLock sl (setLock);

	const bool scratchNow = scratching.load() && loaded != nullptr;

	if (scratchNow != wasScratching)
	{
		if (scratchNow)
		{
			// Hand on the record: start from the current motion, then follow the hand.
			scratchPosition = (double) readPosition.load();
			scratchVelocity = playing.load() ? getEffectiveRate() * fileSampleRate.load() / deviceSampleRate.load() : 0.0;
			scratchGain = playing.load() ? 1.0f : 0.0f;
		}
		else
		{
			resampler.flushBuffers();
		}

		wasScratching = scratchNow;
	}

	if (loaded == nullptr || (! playing.load() && ! scratchNow))
	{
		info.clearActiveBufferRegion();
		return;
	}

	if (scratchNow)
	{
		renderScratch (info);
	}
	else
	{
		updateResamplingRatio(); // follows pitch bend and sync nudge block by block
		resampler.getNextAudioBlock (info);
	}

	// The position is final for this block now: stamped here, the UI carries
	// it forward from this moment (positionAt, PIO sync).
	positionStamp = juce::Time::getMillisecondCounterHiRes() / 1000.0;

	for (int s = 0; s < numStems; ++s)
	{
		auto& smoother = gainSmoothers[(size_t) s];
		smoother.setTargetValue (stemMuted[(size_t) s] ? 0.0f : stemGain[(size_t) s].load());

		const auto from = smoother.getCurrentValue();
		smoother.skip (info.numSamples);
		const auto to = smoother.getCurrentValue();

		float peak = 0.0f;
		double squares = 0.0;

		for (int ch = s * 2; ch < s * 2 + 2; ++ch)
		{
			info.buffer->applyGainRamp (ch, info.startSample, info.numSamples, from, to);
			peak = juce::jmax (peak, info.buffer->getMagnitude (ch, info.startSample, info.numSamples));
			const double rms = info.buffer->getRMSLevel (ch, info.startSample, info.numSamples);
			squares += rms * rms * info.numSamples;
		}

		// The desk's meter, as the stem leaves on its bus: scaled rather than
		// measured on the bus, which sums every stem switched to it.
		deskLevels[(size_t) s].add (remote::sentToBus ({ peak, squares, 2 * info.numSamples }, deckGain.load()));

		peak *= deckGain.load();   // the meter shows what the fader lets through
		if (peak > stemPeak[(size_t) s].load())
			stemPeak[(size_t) s] = peak;
	}
}

// Variable-speed playback in either direction, following scratchTarget.
// Called from getNextAudioBlock with setLock held.
void StemDeckPlayer::renderScratch (const juce::AudioSourceChannelInfo& info)
{
	const auto numSamples = info.numSamples;
	const auto normalSpeed = fileSampleRate.load() / deviceSampleRate.load();
	const auto maxSpeed = maxScratchSpeed * normalSpeed;

	// Velocity eases towards what reaches the target within this block, so
	// hand movements arriving at UI rate become smooth motion.
	const auto wanted = (scratchTarget.load() - scratchPosition) / numSamples;
	scratchVelocity += (wanted - scratchVelocity) * 0.5;
	scratchVelocity = juce::jlimit (-maxSpeed, maxSpeed, scratchVelocity);

	const auto start = scratchPosition;
	const auto end = scratchPosition + scratchVelocity * numSamples;
	const auto first = (juce::int64) std::floor (juce::jmin (start, end)) - 1;
	const auto count = juce::jmin (scratchBuffer.getNumSamples(), (int) (std::ceil (juce::jmax (start, end)) - (double) first) + 3);

	for (int s = 0; s < numStems; ++s)
	{
		auto* reader = loaded->readers[(size_t) s].get();
		float* dest[2] = { scratchBuffer.getWritePointer (s * 2), scratchBuffer.getWritePointer (s * 2 + 1) };

		if (reader->numChannels == 1)
		{
			reader->read (dest, 1, first, count);
			juce::FloatVectorOperations::copy (dest[1], dest[0], count);
		}
		else
		{
			reader->read (dest, 2, first, count);
		}
	}

	// A record held still is silent, not a frozen sample: fade with speed.
	const auto targetGain = (float) juce::jlimit (0.0, 1.0, std::abs (scratchVelocity) / (0.05 * normalSpeed));
	const auto gainStep = (targetGain - scratchGain) / (float) numSamples;

	for (int ch = 0; ch < numOutputChannels; ++ch)
	{
		const auto* source = scratchBuffer.getReadPointer (ch);
		auto* out = info.buffer->getWritePointer (ch, info.startSample);
		auto gain = scratchGain;

		for (int i = 0; i < numSamples; ++i)
		{
			const auto p = start + scratchVelocity * i - (double) first;
			const auto index = juce::jlimit (0, count - 2, (int) p);
			const auto frac = (float) (p - index);
			out[i] = (source[index] + frac * (source[index + 1] - source[index])) * gain;
			gain += gainStep;
		}
	}

	scratchGain = targetGain;
	scratchPosition = juce::jlimit (0.0, (double) loaded->length, end);
	readPosition = (juce::int64) scratchPosition;
}

// Called by the resampler from within getNextAudioBlock, with setLock held.
void StemDeckPlayer::readStems (const juce::AudioSourceChannelInfo& info)
{
	auto* buffer = info.buffer;
	const auto startPos = readPosition.load();
	auto pos = startPos;
	int done = 0;

	while (done < info.numSamples)
	{
		const auto lEnd = loopEnd.load();
		const bool inLoop = lEnd > 0 && pos < lEnd;
		const auto stopAt = inLoop ? lEnd : loaded->length;
		const auto numToRead = (int) juce::jmin ((juce::int64) (info.numSamples - done), stopAt - pos);

		if (numToRead <= 0)
		{
			if (inLoop || (lEnd > 0 && pos == lEnd))
			{
				pos = loopStart.load();
				continue;
			}

			// End of track: start over, or stop.
			if (repeat.load() && loaded->length > 0)
			{
				pos = 0;
				continue;
			}

			for (int ch = 0; ch < numOutputChannels; ++ch)
				buffer->clear (ch, info.startSample + done, info.numSamples - done);

			playing = false;
			break;
		}

		const auto offset = info.startSample + done;

		for (int s = 0; s < numStems; ++s)
		{
			auto* reader = loaded->readers[(size_t) s].get();
			float* dest[2] = { buffer->getWritePointer (s * 2, offset), buffer->getWritePointer (s * 2 + 1, offset) };

			if (reader->numChannels == 1)
			{
				reader->read (dest, 1, pos, numToRead);
				juce::FloatVectorOperations::copy (dest[1], dest[0], numToRead);
			}
			else
			{
				reader->read (dest, 2, pos, numToRead);
			}
		}

		pos += numToRead;
		done += numToRead;

		if (lEnd > 0 && pos == lEnd)
			pos = loopStart.load();
	}

	// If the UI seeked meanwhile, its position wins.
	auto expected = startPos;
	readPosition.compare_exchange_strong (expected, pos);
}

// Background thread: touch the mapped pages ahead of the playhead and around
// the loop start so the audio callback reads from memory, not from disk.
int StemDeckPlayer::useTimeSlice()
{
	std::shared_ptr<LoadedSet> set;

	{
		const juce::ScopedLock sl (setLock);
		set = loaded;
	}

	if (set == nullptr)
		return 100;

	const auto rate = (juce::int64) set->sampleRate;
	const auto touchRange = [&set] (juce::int64 start, juce::int64 numSamples)
	{
		const auto end = juce::jmin (set->length, start + numSamples);

		for (auto* reader : set->mapped)
			if (reader != nullptr)
				for (auto s = juce::jmax ((juce::int64) 0, start); s < end; s += 256)
					reader->touchSample (s);
	};

	touchRange (readPosition.load() - rate * 2, rate * 10); // a little behind too, for scratching

	if (hasLoop())
		touchRange (loopStart.load(), rate * 2);

	return 50;
}
