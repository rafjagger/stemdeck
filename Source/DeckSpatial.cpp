// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "DeckSpatial.h"

#include "SteadyBed.h"
#include "a3-engine/IemNode.h"

#include <cmath>

namespace
{
using FVO = juce::FloatVectorOperations;

constexpr int busChannels = spatial::bformatChannels;
constexpr double rampSeconds = 0.01;
constexpr float iemOrderThree = 4.0f;   // IEM's choice value for 3rd order (0 = auto)

// The per-chunk scratch rows.
enum Row { movingRow, muteRow, cueRow, auxRow, steadyLeft, steadyRight, scratchRows };

// R4 compares against 0.0f and 1.0f on purpose: silence is exactly 0, not "near 0".
bool exactly (float value, float expected)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
	return value == expected;
#pragma GCC diagnostic pop
}

// What a chunk of a gain ramp is: all 0.0f and all 1.0f are shortcuts, and the
// first keeps a silenced path exactly silent.
enum class GainSpan { zero, unity, varying };

GainSpan fill (juce::LinearSmoothedValue<float>& ramp, float* gains, int count)
{
	if (! ramp.isSmoothing())
	{
		const float value = ramp.getTargetValue();
		FVO::fill (gains, value, count);
		return exactly (value, 0.0f) ? GainSpan::zero : (exactly (value, 1.0f) ? GainSpan::unity : GainSpan::varying);
	}
	for (int i = 0; i < count; ++i)
		gains[i] = ramp.getNextValue();
	return GainSpan::varying;
}

bool samePose (const a3::StereoPose& a, const a3::StereoPose& b)
{
	return exactly (a.azimuth, b.azimuth) && exactly (a.elevation, b.elevation) && exactly (a.roll, b.roll)
	    && exactly (a.width, b.width);
}

bool exactlySilent (const float* samples, int count)
{
	const auto range = FVO::findMinAndMax (samples, count);
	return exactly (range.getStart(), 0.0f) && exactly (range.getEnd(), 0.0f);
}

bool active (const juce::LinearSmoothedValue<float>& ramp)
{
	return ramp.isSmoothing() || ! exactly (ramp.getTargetValue(), 0.0f);
}

void addScaled (float* destination, const float* source, const float* gains, GainSpan span, int count)
{
	switch (span)
	{
		case GainSpan::zero: return;
		case GainSpan::unity: FVO::add (destination, source, count); return;
		case GainSpan::varying: FVO::addWithMultiply (destination, source, gains, count); return;
	}
}

float wrapAzimuth (float degrees)
{
	if (! std::isfinite (degrees))
		return 0.0f;
	return degrees - 360.0f * std::floor ((degrees + 180.0f) / 360.0f);
}

float finiteOr (float value, float fallback)
{
	return std::isfinite (value) ? value : fallback;
}

void copyIfWanted (float* destination, const float* source, int count)
{
	if (destination != nullptr)
		FVO::copy (destination, source, count);
}

// A view of the first count samples: JUCE's preallocated channel table, no allocation.
void runNode (a3::IemNode& node, juce::AudioBuffer<float>& buffer, int count)
{
	juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), count);
	node.process (chunk);
}

std::atomic<int> decodersCreated { 0 };

std::unique_ptr<a3::IemNode> binauralDecoder (juce::String& error)
{
	++decodersCreated;
	auto decoder = a3::IemNode::create (a3::IemKind::binauralDecoder, busChannels, 2);
	if (decoder == nullptr)
	{
		error = "IEM refused a BinauralDecoder";
		return nullptr;
	}
	auto* parameters = new juce::DynamicObject();
	parameters->setProperty ("inputOrderSetting", iemOrderThree);
	parameters->setProperty ("useSN3D", 1.0f);
	parameters->setProperty ("applyHeadphoneEq", 0.0f);
	const auto set = decoder->setParameters (juce::var (parameters));
	if (set.failed())
	{
		error = "BinauralDecoder: " + set.getErrorMessage();
		return nullptr;
	}
	return decoder;
}

// A vDSO clock read either side of a node: kept in production so S1.4/S2 can report per-node figures.
template <typename Work>
void timed (std::int64_t& ticks, Work&& work)
{
	const auto start = juce::Time::getHighResolutionTicks();
	work();
	ticks += juce::Time::getHighResolutionTicks() - start;
}

// The send's ramp times the stem's MUTE ramp, per sample; zero if either is 0 throughout.
GainSpan sendGain (juce::LinearSmoothedValue<float>& sendRamp, GainSpan muteSpan, const float* mute, float* send, int count)
{
	const GainSpan sendSpan = fill (sendRamp, send, count);
	if (sendSpan == GainSpan::zero || muteSpan == GainSpan::zero)
		return GainSpan::zero;
	if (muteSpan == GainSpan::varying)
		FVO::multiply (send, mute, count);
	return GainSpan::varying;
}
}

struct DeckSpatial::Stem
{
	std::unique_ptr<a3::IemNode> encoder;
	juce::AudioBuffer<float> io;   // 2 in, 16 out
	Ramp moving, mute, cue, aux;
	a3::StereoPose pose {};        // the pose last handed to the encoder
	int glideRemaining = 0;        // samples IEM's high-quality smoothing still glides towards it

	bool rampsSettled() const { return ! (moving.isSmoothing() || mute.isSmoothing() || cue.isSmoothing() || aux.isSmoothing()); }
};

DeckSpatial::DeckSpatial() = default;
DeckSpatial::~DeckSpatial() = default;

std::unique_ptr<DeckSpatial> DeckSpatial::create (juce::String& error)
{
	std::unique_ptr<DeckSpatial> deck (new DeckSpatial());
	for (auto& stem : deck->stemNodes)
	{
		stem = std::make_unique<Stem>();
		stem->encoder = a3::IemNode::create (a3::IemKind::stereoEncoder, 2, busChannels);
		if (stem->encoder == nullptr)
		{
			error = "IEM refused a StereoEncoder";
			return nullptr;
		}
	}
	deck->mainBed = SteadyBed::create (error);
	deck->cueBed = deck->mainBed != nullptr ? SteadyBed::create (error) : nullptr;
	deck->auxBed = deck->cueBed != nullptr ? SteadyBed::create (error) : nullptr;
	if (deck->auxBed == nullptr)
		return nullptr;
	deck->phonesDecoder = binauralDecoder (error);
	if (deck->phonesDecoder == nullptr)
		return nullptr;
	return deck;
}

int DeckSpatial::binauralDecodersCreated()
{
	return decodersCreated.load();
}

DeckSpatial::Controls& DeckSpatial::controls()
{
	return state;
}

a3::StereoPose DeckSpatial::poseOf (int stem) const
{
	const auto s = (size_t) stem;
	return { wrapAzimuth (state.azimuth[s].load (std::memory_order_relaxed)),
	         finiteOr (state.elevation[s].load (std::memory_order_relaxed), 0.0f), 0.0f,
	         finiteOr (state.width[s].load (std::memory_order_relaxed), spatial::Pose{}.width) };
}

void DeckSpatial::prepare (double sampleRate, int newBlockSize)
{
	blockSize = newBlockSize;
	for (int s = 0; s < stems; ++s)
	{
		auto& stem = *stemNodes[(size_t) s];
		// Before prepare: IEM's own width default is 0, and the warm-up block
		// settles the encoder's smoothing on the stem's pose.
		stem.pose = poseOf (s);
		stem.glideRemaining = 0;
		stem.encoder->setStereoPose (stem.pose);
		stem.encoder->prepare (sampleRate, blockSize);
		stem.io.setSize (busChannels, blockSize);
		for (auto* ramp : { &stem.moving, &stem.mute, &stem.cue, &stem.aux })
			ramp->reset (sampleRate, rampSeconds);
	}
	mainBed->prepare (sampleRate, blockSize);
	cueBed->prepare (sampleRate, blockSize);
	auxBed->prepare (sampleRate, blockSize);
	phonesDecoder->prepare (sampleRate, blockSize);
	for (auto* buffer : { &mainSum, &cueSum, &auxSum, &bedOut, &auxBedOut, &phonesDecoderIo })
		buffer->setSize (busChannels, blockSize);
	for (auto* buffer : { &bedIn, &cueBedIn, &auxBedIn })
		buffer->setSize (2, blockSize);
	scratch.setSize (scratchRows, blockSize);
	phonesCueWeight.reset (sampleRate, rampSeconds);
	phonesMixWeight.reset (sampleRate, rampSeconds);

	// A fresh deck plays at its settings at once; only the cue fades in, as it
	// does whenever a stem goes on cue.
	readControls();
	for (auto& stem : stemNodes)
	{
		stem->moving.setCurrentAndTargetValue (stem->moving.getTargetValue());
		stem->mute.setCurrentAndTargetValue (stem->mute.getTargetValue());
		stem->cue.setCurrentAndTargetValue (0.0f);
		stem->aux.setCurrentAndTargetValue (stem->aux.getTargetValue());
	}
	setPhonesTargets (false);
	phonesCueWeight.setCurrentAndTargetValue (phonesCueWeight.getTargetValue());
	phonesMixWeight.setCurrentAndTargetValue (phonesMixWeight.getTargetValue());
	phonesGate.reset();
	auxGate.reset();
	phonesDecoderStale = false;
	phonesDecoding = false;
	phonesPrimeRemaining = 0;
	resetTiming();
}

void DeckSpatial::readControls()
{
	const float deckThreeD = state.deckThreeD.load (std::memory_order_relaxed);
	for (int s = 0; s < stems; ++s)
	{
		const auto i = (size_t) s;
		auto& stem = *stemNodes[i];
		const float threeD = spatial::effectiveThreeD (deckThreeD, state.threeD[i].load (std::memory_order_relaxed),
		                                                 state.envelope[i].load (std::memory_order_relaxed));
		stem.moving.setTargetValue (spatial::movingGain (threeD));
		stem.mute.setTargetValue (state.muted[i].load (std::memory_order_relaxed) ? 0.0f : 1.0f);
		stem.cue.setTargetValue (state.cued[i].load (std::memory_order_relaxed) ? 1.0f : 0.0f);
		stem.aux.setTargetValue (spatial::auxGain (state.aux[i].load (std::memory_order_relaxed)));
		const auto pose = poseOf (s);
		if (! samePose (pose, stem.pose))
		{
			// IEM's StereoEncoder glides to a new pose over the block it was
			// prepared with: smoothAzimuthL.reset (1, samplesPerBlock) and its
			// kin in its prepareToPlay (PluginProcessor.cpp:124-127).
			stem.pose = pose;
			stem.glideRemaining = stem.encoder->preparedBlockSize();
		}
		stem.encoder->setStereoPose (pose);
	}
	deckPlaying = state.playing.load (std::memory_order_relaxed);
	phonesSettings = spatial::phonesWeights (state.phonesLevel.load (std::memory_order_relaxed),
	                                         state.phonesMix.load (std::memory_order_relaxed));
	phonesDecoderOn = state.phonesDecoderOn.load (std::memory_order_relaxed);
}

// Two-deck mode's binaural cue is the decoder at cue 1, mix 0 (R2), whatever
// the phones settings; otherwise the phones' own, and with the decoder off
// both ramp to 0, so it stops through its hold-over.
void DeckSpatial::setPhonesTargets (bool binauralCue)
{
	if (binauralCue)
	{
		phonesCueWeight.setTargetValue (1.0f);
		phonesMixWeight.setTargetValue (0.0f);
		return;
	}
	phonesCueWeight.setTargetValue (phonesDecoderOn ? phonesSettings.cue : 0.0f);
	phonesMixWeight.setTargetValue (phonesDecoderOn ? phonesSettings.mix : 0.0f);
}

void DeckSpatial::process (const float* const* stemsPreMute, const Outputs& out, int numSamples)
{
	jassert (blockSize > 0);
	if (blockSize <= 0 || numSamples <= 0)
		return;
	juce::ScopedNoDenormals noDenormals;
	++timedBlocks;
	for (int offset = 0; offset < numSamples; offset += blockSize)
		processChunk (stemsPreMute, out, offset, juce::jmin (blockSize, numSamples - offset));
}

// R12: the aux bed runs while any send is above 0 or still moving; its
// hold-over (auxGate) covers the bed's smoothing after the last one.
bool DeckSpatial::auxWanted() const
{
	for (const auto& stem : stemNodes)
		if (active (stem->aux))
			return true;
	return false;
}

bool DeckSpatial::cueWanted() const
{
	for (const auto& stem : stemNodes)
		if (active (stem->cue))
			return true;
	return false;
}

void DeckSpatial::processChunk (const float* const* stemsPreMute, const Outputs& out, int offset, int count)
{
	readControls();
	const bool standing = stands (stemsPreMute, offset, count);
	const bool cueInUse = ! standing && cueWanted();
	for (auto* buffer : { &mainSum, &cueSum, &auxSum, &bedIn, &cueBedIn, &auxBedIn })
		buffer->clear (0, count);
	if (standing)
	{
		renderSilentMain (out, offset, count);
	}
	else
	{
		for (int s = 0; s < stems; ++s)
			encodeStem (*stemNodes[(size_t) s], stemsPreMute[2 * s] + offset, stemsPreMute[2 * s + 1] + offset, count);
		renderMain (out, offset, count);
		renderAux (out, offset, count);
	}
	renderCue (out, offset, count, cueInUse);
	renderPhones (out, offset, count, cueInUse);
}

// R14: not playing, every stem's pre-mute input exactly 0.0f, no gain ramp
// moving. Every path is then linear in silence: encoders and beds would add 0.
bool DeckSpatial::stands (const float* const* stemsPreMute, int offset, int count) const
{
	if (deckPlaying)
		return false;
	for (const auto& stem : stemNodes)
		if (! stem->rampsSettled())
			return false;
	for (int c = 0; c < 2 * stems; ++c)
		if (! exactlySilent (stemsPreMute[c] + offset, count))
			return false;
	return true;
}

// A standing deck's ports are exactly 0.0f. An encoder whose pose moved runs
// once on silence, so the deck resumes at that pose instead of gliding from
// the last one it encoded (a deck that never stood glided on its silence).
void DeckSpatial::renderSilentMain (const Outputs& out, int offset, int count)
{
	for (auto& stem : stemNodes)
		finishGlide (*stem);
	for (auto* port : out.bformat)
		if (port != nullptr)
			FVO::clear (port + offset, count);
	for (auto* port : out.aux)
		if (port != nullptr)
			FVO::clear (port + offset, count);
	auxGate.reset();   // standing, every send is settled and the bed has nothing to add
}

void DeckSpatial::finishGlide (Stem& stem)
{
	if (stem.glideRemaining <= 0)
		return;
	const int remaining = juce::jmin (stem.glideRemaining, blockSize);
	stem.io.clear (0, remaining);
	timed (encoderClock.ticks, [&] { runNode (*stem.encoder, stem.io, remaining); });
	stem.glideRemaining = 0;
}

// Moving share g·x into the stem's StereoEncoder, steady share x - g·x into the
// beds; MUTE on the encoder's output and the bed share, the cue pre-mute.
void DeckSpatial::encodeStem (Stem& stem, const float* left, const float* right, int count)
{
	float* moving = scratch.getWritePointer (movingRow);
	float* mute = scratch.getWritePointer (muteRow);
	float* cue = scratch.getWritePointer (cueRow);
	float* send = scratch.getWritePointer (auxRow);
	const GainSpan movingSpan = fill (stem.moving, moving, count);
	const GainSpan muteSpan = fill (stem.mute, mute, count);
	const GainSpan cueSpan = fill (stem.cue, cue, count);
	// The send acts after MUTE: both ramps in one per-sample gain.
	const GainSpan sendSpan = sendGain (stem.aux, muteSpan, mute, send, count);

	const float* const in[2] = { left, right };
	float* const steady[2] = { scratch.getWritePointer (steadyLeft), scratch.getWritePointer (steadyRight) };
	for (int side = 0; side < 2; ++side)
	{
		float* encoderIn = stem.io.getWritePointer (side);
		if (movingSpan == GainSpan::zero)
			FVO::clear (encoderIn, count);
		else
			FVO::multiply (encoderIn, in[side], moving, count);
		FVO::subtract (steady[side], in[side], encoderIn, count);
	}
	timed (encoderClock.ticks, [&] { runNode (*stem.encoder, stem.io, count); });
	stem.glideRemaining = juce::jmax (0, stem.glideRemaining - count);

	for (int c = 0; c < busChannels; ++c)
	{
		addScaled (mainSum.getWritePointer (c), stem.io.getReadPointer (c), mute, muteSpan, count);
		if (c < cueChannels)
			addScaled (cueSum.getWritePointer (c), stem.io.getReadPointer (c), cue, cueSpan, count);
		addScaled (auxSum.getWritePointer (c), stem.io.getReadPointer (c), send, sendSpan, count);
	}
	for (int side = 0; side < 2; ++side)
	{
		addScaled (bedIn.getWritePointer (side), steady[side], mute, muteSpan, count);
		addScaled (cueBedIn.getWritePointer (side), steady[side], cue, cueSpan, count);
		addScaled (auxBedIn.getWritePointer (side), steady[side], send, sendSpan, count);
	}
}

void DeckSpatial::renderMain (const Outputs& out, int offset, int count)
{
	timed (bedClock.ticks, [&] { mainBed->process (bedIn.getReadPointer (0), bedIn.getReadPointer (1), bedOut, count); });
	for (int c = 0; c < busChannels; ++c)
	{
		float* sum = mainSum.getWritePointer (c);
		FVO::add (sum, bedOut.getReadPointer (c), count);
		FVO::multiply (sum, spatial::trim, count);
		copyIfWanted (out.bformat[(size_t) c] == nullptr ? nullptr : out.bformat[(size_t) c] + offset, sum, count);
	}
}

// The stems' sends: their encoder output (auxSum) plus the steady share through
// the aux bed. With every send at 0 the bed is not called and the bus is 0.0f.
void DeckSpatial::renderAux (const Outputs& out, int offset, int count)
{
	const bool bedRuns = auxGate.next (auxWanted(), count);
	if (bedRuns)
	{
		auxBed->process (auxBedIn.getReadPointer (0), auxBedIn.getReadPointer (1), auxBedOut, count);
		auxBedSamples += count;
	}
	for (int c = 0; c < busChannels; ++c)
	{
		float* sum = auxSum.getWritePointer (c);
		if (bedRuns)
			FVO::add (sum, auxBedOut.getReadPointer (c), count);
		FVO::multiply (sum, spatial::trim, count);
		copyIfWanted (out.aux[(size_t) c] == nullptr ? nullptr : out.aux[(size_t) c] + offset, sum, count);
	}
}

// The cued stems in isolation, 2nd order: SN3D's ACN 0..8 of the 3rd-order cue
// sum are the 2nd-order signal, so this truncates, it does not re-encode (M4).
// Nothing cued, the cue bed is not run and the sum is 0.0f.
void DeckSpatial::renderCue (const Outputs& out, int offset, int count, bool wanted)
{
	if (wanted)
		timed (cueBedClock.ticks, [&] { cueBed->process (cueBedIn.getReadPointer (0), cueBedIn.getReadPointer (1), bedOut, count); });
	for (int c = 0; c < cueChannels; ++c)
	{
		float* sum = cueSum.getWritePointer (c);
		if (wanted)
		{
			FVO::add (sum, bedOut.getReadPointer (c), count);
			FVO::multiply (sum, spatial::trim, count);
		}
		copyIfWanted (out.cue[(size_t) c] == nullptr ? nullptr : out.cue[(size_t) c] + offset, sum, count);
	}
}

// The deck's one decoder: mixWeight * main16 + cueWeight * cue9, ACN 9..15 of
// the cue left out. It runs only while a rendered pair (phones, or two-deck's
// binaural cue) takes it and it has something to decode; turned off or
// emptied it gets a hold-over, unplugged it stops at once (R12). Its next
// start after such a hard stop first primes it with silence, a block per
// chunk, before it decodes the deck again.
void DeckSpatial::renderPhones (const Outputs& out, int offset, int count, bool cueInUse)
{
	const auto both = [] (const std::array<float*, 2>& pair) { return pair[0] != nullptr && pair[1] != nullptr; };
	const bool binauralCue = both (out.binauralCue);
	if (! binauralCue && ! both (out.phones))
	{
		phonesCueWeight.skip (count);
		phonesMixWeight.skip (count);
		phonesGate.reset();
		phonesDecoding = false;
		for (const auto* pair : { &out.phones, &out.binauralCue })
			for (auto* port : *pair)
				if (port != nullptr)
					FVO::clear (port + offset, count);
		return;
	}

	setPhonesTargets (binauralCue);
	const bool wanted = active (phonesMixWeight) || (cueInUse && active (phonesCueWeight));
	float* cueWeight = scratch.getWritePointer (movingRow);
	float* mixWeight = scratch.getWritePointer (muteRow);
	const GainSpan cueSpan = fill (phonesCueWeight, cueWeight, count);
	const GainSpan mixSpan = fill (phonesMixWeight, mixWeight, count);
	const bool decoding = phonesGate.next (wanted, count);
	const bool starting = decoding && ! phonesDecoding;
	if (phonesDecoding && ! decoding)
		phonesDecoderStale = false;   // a hold-over that ran out leaves it empty
	phonesDecoding = decoding;
	if (starting && phonesDecoderStale)
	{
		phonesPrimeRemaining = DecoderGate::holdOverSamples;
		phonesDecoderStale = false;
	}

	bool decodedTheDeck = false;
	if (decoding && phonesPrimeRemaining > 0)
	{
		// Costs one extra decoder block per chunk, for 2048 samples after a reconnect.
		primePhonesDecoderOneBlock();
	}
	else if (decoding)
	{
		phonesDecoderIo.clear (0, count);
		if (wanted)
		{
			for (int c = 0; c < busChannels; ++c)
				addScaled (phonesDecoderIo.getWritePointer (c), mainSum.getReadPointer (c), mixWeight, mixSpan, count);
			if (cueInUse)
				for (int c = 0; c < cueChannels; ++c)
					addScaled (phonesDecoderIo.getWritePointer (c), cueSum.getReadPointer (c), cueWeight, cueSpan, count);
		}
		timed (phonesDecoderClock.ticks, [&] { runNode (*phonesDecoder, phonesDecoderIo, count); });
		phonesDecoderSamples += count;
		phonesDecoderStale = true;   // until a hold-over runs out
		decodedTheDeck = wanted;
	}
	for (const auto* pair : { &out.phones, &out.binauralCue })
		for (int side = 0; side < 2; ++side)
		{
			float* port = (*pair)[(size_t) side];
			if (port == nullptr)
				continue;
			if (decodedTheDeck)
				FVO::copy (port + offset, phonesDecoderIo.getReadPointer (side), count);
			else
				FVO::clear (port + offset, count);
		}
}

// After a hard stop the decoder still holds the tail of what it last heard: a
// hold-over's worth of silence, output discarded, empties it. One block per
// chunk, so a reconnect never runs more than two decoder blocks in one chunk.
void DeckSpatial::primePhonesDecoderOneBlock()
{
	const int primeCount = juce::jmin (blockSize, phonesPrimeRemaining);
	phonesDecoderIo.clear (0, primeCount);
	timed (phonesDecoderClock.ticks, [&] { runNode (*phonesDecoder, phonesDecoderIo, primeCount); });
	phonesDecoderSamples += primeCount;
	phonesPrimeRemaining -= primeCount;
}

DeckSpatial::Timing DeckSpatial::timing() const
{
	const auto mean = [this] (const Clock& clock)
	{
		if (timedBlocks == 0)
			return 0.0;
		return juce::Time::highResolutionTicksToSeconds (clock.ticks) * 1.0e6 / (double) timedBlocks;
	};
	return { mean (encoderClock), mean (bedClock), mean (cueBedClock), mean (phonesDecoderClock) };
}

DeckSpatial::DecoderRuns DeckSpatial::decoderRuns() const
{
	return { phonesDecoderSamples, auxBedSamples };
}

void DeckSpatial::resetTiming()
{
	for (auto* clock : { &encoderClock, &bedClock, &cueBedClock, &phonesDecoderClock })
		clock->ticks = 0;
	timedBlocks = 0;
	phonesDecoderSamples = 0;
	auxBedSamples = 0;
}

float DeckSpatial::movingGainNow (int stem) const
{
	return stemNodes[(size_t) stem]->moving.getCurrentValue();
}
