#include "StemDeckPlayer.h"
#include "Remote.h"
#include "GridEdit.h"
#include "Buses.h"

// The session keeps one mix state per stem (Session.h counts them as the buses do).
static_assert (StemDeckPlayer::numStems == buses::stemsPerDeck);

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
	loopEnd = 0;
	gridBpm = 0.0;
	loopStart = 0;
	cuePoint = 0.0;
	fileSampleRate = newSet->sampleRate;
	lengthInSamples = newSet->length;
	seekTo (0);

	{
		const juce::ScopedLock sl (setLock);
		std::swap (loaded, newSet);
	}

	// The previous set (now in newSet) is released here, outside the lock.
	resampler.flushBuffers();
	updateResamplingRatio();

	if (keyLock.load())
		buildStretchers();   // for this file's sample rate
	return {};
}

void StemDeckPlayer::play()
{
	if (! isLoaded())
		return;

	if (readPosition.load() >= lengthInSamples.load())
		seekTo (hasLoop() ? loopStart.load() : 0);

	playing = true;
}

void StemDeckPlayer::pause()
{
	playing = false;
}

void StemDeckPlayer::stop()
{
	playing = false;
	seekTo (hasLoop() ? loopStart.load() : 0);
	resampler.flushBuffers();
}

void StemDeckPlayer::setPosition (double seconds)
{
	seekTo (juce::jlimit ((juce::int64) 0, lengthInSamples.load(), (juce::int64) (seconds * fileSampleRate.load())));
	resampler.flushBuffers();
}

// Written here for the views, and handed to the audio thread so the key
// lock path starts again from there; the plain path reads readPosition.
void StemDeckPlayer::seekTo (juce::int64 position)
{
	readPosition = position;
	pendingSeek = position;
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

void StemDeckPlayer::setKeyLock (bool on)
{
	if (on)
		buildStretchers();

	keyLock = on;
}

StemDeckPlayer::Stretchers::Stretchers (double fileRate) : sampleRate (fileRate)
{
	const auto alignment = KeyLockStretcher::measureAlignment (fileRate);
	for (auto& stem : stems)
		stem = std::make_unique<KeyLockStretcher> (2, fileRate, alignment);
}

// Builds (and measures) the stretchers for the loaded file's sample rate,
// unless they are there; allocates, so never on the audio thread.
void StemDeckPlayer::buildStretchers()
{
	const auto rate = fileSampleRate.load();

	if (stretchers != nullptr && juce::approximatelyEqual (stretchers->sampleRate, rate))
		return;

	auto fresh = std::make_unique<Stretchers> (rate);

	{
		const juce::ScopedLock sl (setLock);
		std::swap (stretchers, fresh);
	}
	// The old ones (now in fresh) go here, outside the lock.
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
	keyLockResampler.prepareToPlay (samplesPerBlockExpected, sampleRate);

	// Room for one block at the highest scratch speed (plus interpolation margin).
	const auto maxBlock = juce::jmax (samplesPerBlockExpected, 4096);
	scratchBuffer.setSize (numOutputChannels, (int) (maxBlock * maxScratchSpeed * 4) + 8);

	// Key lock on or off while playing: the two paths crossfade, short
	// enough to sound like a switch, long enough not to click.
	fadeBuffer.setSize (numOutputChannels, maxBlock);
	fadeLength = juce::jmax (1, juce::roundToInt (sampleRate * 0.02));

	for (int i = 0; i < numStems; ++i)
	{
		gainSmoothers[(size_t) i].reset (sampleRate, 0.02);
		gainSmoothers[(size_t) i].setCurrentAndTargetValue (stemMuted[(size_t) i] ? 0.0f : stemGain[(size_t) i].load());
	}
}

void StemDeckPlayer::releaseResources()
{
	resampler.releaseResources();
	keyLockResampler.releaseResources();
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
			restartPending = true;   // the key lock path goes on from where the hand left the record
		}

		wasScratching = scratchNow;
	}

	if (loaded == nullptr || (! playing.load() && ! scratchNow))
	{
		info.clearActiveBufferRegion();
		return;
	}

	if (scratchNow)
		renderScratch (info);
	else
		renderPlayback (info);

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

		// One measurement, two meters. The desk's: as the stem leaves on its
		// bus, scaled rather than measured on the bus, which sums every stem
		// switched to it. The screen's, behind the deck's volume fader: before
		// that fader, as a DJ mixer's channel meter.
		const remote::Block afterKnob { peak, squares, 2 * info.numSamples };
		deskLevels[(size_t) s].add (remote::sentToBus (afterKnob, deckGain.load()));
		const auto screenPeak = remote::beforeFader (afterKnob).peak;
		if (screenPeak > stemPeak[(size_t) s].load())
			stemPeak[(size_t) s] = screenPeak;
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

		std::array<float*, numOutputChannels> dest;
		for (int ch = 0; ch < numOutputChannels; ++ch)
			dest[(size_t) ch] = buffer->getWritePointer (ch, info.startSample + done);
		readRun (dest.data(), numToRead, pos);

		pos += numToRead;
		done += numToRead;

		if (lEnd > 0 && pos == lEnd)
			pos = loopStart.load();
	}

	// If the UI seeked meanwhile, its position wins.
	auto expected = startPos;
	readPosition.compare_exchange_strong (expected, pos);
}

// Every stem's `count` frames from `position` on, stem N into dest[2N], dest[2N+1].
// Audio thread, with setLock held.
void StemDeckPlayer::readRun (float* const* dest, int count, juce::int64 position)
{
	for (int s = 0; s < numStems; ++s)
		readStemRun (s, dest + s * 2, count, position);
}

// One stem's `count` frames into dest[0], dest[1]; a mono stem on both.
void StemDeckPlayer::readStemRun (int stem, float* const* dest, int count, juce::int64 position)
{
	auto* reader = loaded->readers[(size_t) stem].get();
	float* pair[2] = { dest[0], dest[1] };

	if (reader->numChannels == 1)
	{
		reader->read (pair, 1, position, count);
		juce::FloatVectorOperations::copy (pair[1], pair[0], count);
	}
	else
	{
		reader->read (pair, 2, position, count);
	}
}

//==============================================================================
// Playing, not scratching: the plain path or the key lock path, and for
// fadeLength after a switch both, crossfaded. Audio thread, setLock held.
void StemDeckPlayer::renderPlayback (const juce::AudioSourceChannelInfo& info)
{
	const auto seek = pendingSeek.exchange (-1);
	const bool ready = stretchers != nullptr && juce::approximatelyEqual (stretchers->sampleRate, fileSampleRate.load());
	const bool wanted = keylock::usesStretcher (keyLock.load(), ready, false);

	if (stretchers.get() != stretchersInUse)
	{
		stretchersInUse = stretchers.get();
		restartPending = true;
	}

	if (stretching && ! ready)
	{
		stretching = false;   // its stretchers are gone: the plain path, as it is
		fadeRemaining = 0;
	}

	if (seek >= 0)
		fadeRemaining = 0;   // a jump is a cut anyway

	if (stretching && (seek >= 0 || restartPending))
		restartStretchers (seek >= 0 ? seek : readPosition.load());

	restartPending = false;

	if (wanted != stretching)
	{
		if (wanted)
		{
			restartStretchers (readPosition.load());   // takes over where the plain path is
		}
		else
		{
			// The plain path goes on from what is heard.
			readPosition = juce::jlimit ((juce::int64) 0, loaded->length, (juce::int64) stretchedPosition());
			resampler.flushBuffers();
		}

		stretching = wanted;
		fadeRemaining = fadeLength;
	}

	if (fadeRemaining > 0 && info.numSamples <= fadeBuffer.getNumSamples())
	{
		const juce::AudioSourceChannelInfo outgoing { &fadeBuffer, 0, info.numSamples };
		renderPath (! stretching, outgoing);
		renderPath (stretching, info);

		for (int ch = 0; ch < numOutputChannels; ++ch)
		{
			const auto* from = fadeBuffer.getReadPointer (ch);
			auto* to = info.buffer->getWritePointer (ch, info.startSample);

			for (int i = 0; i < info.numSamples; ++i)
			{
				const auto in = juce::jlimit (0.0f, 1.0f, 1.0f - (float) (fadeRemaining - i) / (float) fadeLength);
				to[i] = to[i] * in + from[i] * (1.0f - in);
			}
		}

		fadeRemaining = juce::jmax (0, fadeRemaining - info.numSamples);
	}
	else
	{
		fadeRemaining = 0;
		renderPath (stretching, info);
	}

	// While the plain path still fades out, it owns the position; then the
	// stretchers report what is heard. A seek meanwhile wins.
	if (stretching && fadeRemaining == 0)
	{
		const auto heard = stretchedPosition();

		if (stretchers->stems[0]->hasEnded() && heard >= (double) loaded->length)
			playing = false;

		if (pendingSeek.load() < 0)
			readPosition = juce::jlimit ((juce::int64) 0, loaded->length, (juce::int64) heard);
	}
}

void StemDeckPlayer::renderPath (bool stretched, const juce::AudioSourceChannelInfo& info)
{
	if (stretched)
	{
		keyLockResampler.setResamplingRatio (keylock::resamplingRatio (getEffectiveRate(), fileSampleRate.load(), deviceSampleRate.load(), true));
		keyLockResampler.getNextAudioBlock (info);
	}
	else
	{
		updateResamplingRatio(); // follows pitch bend and sync nudge block by block
		resampler.getNextAudioBlock (info);
	}
}

// Called by keyLockResampler from within renderPath, with setLock held.
void StemDeckPlayer::renderStretched (const juce::AudioSourceChannelInfo& info)
{
	const auto rate = getEffectiveRate();

	for (int s = 0; s < numStems; ++s)
	{
		float* pair[2] = { info.buffer->getWritePointer (s * 2, info.startSample),
						   info.buffer->getWritePointer (s * 2 + 1, info.startSample) };
		stretchers->stems[(size_t) s]->render (pair, info.numSamples, rate, keyLockInputs[(size_t) s]);
	}
}

void StemDeckPlayer::restartStretchers (juce::int64 position)
{
	const auto rate = getEffectiveRate();

	for (int s = 0; s < numStems; ++s)
		stretchers->stems[(size_t) s]->restart (position, rate, keyLockInputs[(size_t) s]);

	keyLockResampler.flushBuffers();
}

// The first stem's playhead stands for the deck's: the stems' agree to a
// fraction of a millisecond, and around a loop's end an average of them
// would be neither end.
double StemDeckPlayer::stretchedPosition() const
{
	return stretchers->stems[0]->position();
}

// As readStems() reads: up to the loop's end while inside the loop, else
// up to the track's end.
int StemDeckPlayer::KeyLockInput::read (float* const* dest, int count, std::int64_t position)
{
	const auto loopEnd = owner.loopEnd.load();
	const auto stopAt = loopEnd > 0 && position < loopEnd ? loopEnd : owner.loaded->length;
	const auto n = (int) juce::jlimit ((juce::int64) 0, (juce::int64) count, stopAt - position);

	if (n > 0)
		owner.readStemRun (stem, dest, n, position);
	return n;
}

std::int64_t StemDeckPlayer::KeyLockInput::jumpFrom (std::int64_t position)
{
	const auto loopEnd = owner.loopEnd.load();

	if (loopEnd > 0 && position == loopEnd)
		return owner.loopStart.load();
	if (position >= owner.loaded->length && owner.repeat.load() && owner.loaded->length > 0)
		return 0;
	return -1;
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
