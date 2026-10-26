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
enum Row { movingRow, muteRow, cueRow, steadyLeft, steadyRight, scratchRows };

// What a chunk of a gain ramp is: all 0.0f and all 1.0f are shortcuts, and the
// first keeps a silenced path exactly silent.
enum class GainSpan { zero, unity, varying };

GainSpan fill (juce::LinearSmoothedValue<float>& ramp, float* gains, int count)
{
	if (! ramp.isSmoothing())
	{
		const float value = ramp.getTargetValue();
		FVO::fill (gains, value, count);
		return value == 0.0f ? GainSpan::zero : (value == 1.0f ? GainSpan::unity : GainSpan::varying);
	}
	for (int i = 0; i < count; ++i)
		gains[i] = ramp.getNextValue();
	return GainSpan::varying;
}

bool active (const juce::LinearSmoothedValue<float>& ramp)
{
	return ramp.isSmoothing() || ramp.getTargetValue() != 0.0f;
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

std::unique_ptr<a3::IemNode> binauralDecoder (juce::String& error)
{
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

template <typename Work>
void timed (std::int64_t& ticks, Work&& work)
{
	const auto start = juce::Time::getHighResolutionTicks();
	work();
	ticks += juce::Time::getHighResolutionTicks() - start;
}
}

struct DeckSpatial::Stem
{
	std::unique_ptr<a3::IemNode> encoder;
	juce::AudioBuffer<float> io;   // 2 in, 16 out
	Ramp moving, mute, cue;
};

DeckSpatial::Controls::Controls()
{
	for (auto& w : width)
		w = spatial::Pose{}.width;
}

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
	if (deck->cueBed == nullptr)
		return nullptr;
	deck->cueDecoder = binauralDecoder (error);
	deck->phonesDecoder = deck->cueDecoder != nullptr ? binauralDecoder (error) : nullptr;
	if (deck->phonesDecoder == nullptr)
		return nullptr;
	return deck;
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
		stem.encoder->setStereoPose (poseOf (s));
		stem.encoder->prepare (sampleRate, blockSize);
		stem.io.setSize (busChannels, blockSize);
		for (auto* ramp : { &stem.moving, &stem.mute, &stem.cue })
			ramp->reset (sampleRate, rampSeconds);
	}
	mainBed->prepare (sampleRate, blockSize);
	cueBed->prepare (sampleRate, blockSize);
	cueDecoder->prepare (sampleRate, blockSize);
	phonesDecoder->prepare (sampleRate, blockSize);
	for (auto* buffer : { &mainSum, &cueSum, &bedOut, &cueDecoderIo, &phonesDecoderIo })
		buffer->setSize (busChannels, blockSize);
	for (auto* buffer : { &bedIn, &cueBedIn, &auxSum, &cueOut })
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
	}
	phonesCueWeight.setCurrentAndTargetValue (phonesCueWeight.getTargetValue());
	phonesMixWeight.setCurrentAndTargetValue (phonesMixWeight.getTargetValue());
	cueGate.reset();
	phonesGate.reset();
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
		stem.encoder->setStereoPose (poseOf (s));
	}
	const auto weights = spatial::phonesWeights (state.phonesLevel.load (std::memory_order_relaxed),
	                                             state.phonesMix.load (std::memory_order_relaxed));
	phonesCueWeight.setTargetValue (weights.cue);
	phonesMixWeight.setTargetValue (state.phonesDecoderOn.load (std::memory_order_relaxed) ? weights.mix : 0.0f);
}

void DeckSpatial::process (const float* const* stemsPreMute, const Outputs& out, int numSamples)
{
	jassert (blockSize > 0);
	if (blockSize <= 0)
		return;
	juce::ScopedNoDenormals noDenormals;
	++timedBlocks;
	for (int offset = 0; offset < numSamples; offset += blockSize)
		processChunk (stemsPreMute, out, offset, juce::jmin (blockSize, numSamples - offset));
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
	const bool cueInUse = cueWanted();
	for (auto* buffer : { &mainSum, &cueSum, &bedIn, &cueBedIn, &auxSum })
		buffer->clear (0, count);
	for (int s = 0; s < stems; ++s)
		encodeStem (*stemNodes[(size_t) s], stemsPreMute[2 * s] + offset, stemsPreMute[2 * s + 1] + offset, count);
	renderMain (out, offset, count);
	renderCue (out, offset, count, cueInUse);
	renderPhones (out, offset, count);
}

// Moving share g·x into the stem's StereoEncoder, steady share x - g·x into the
// beds; MUTE on the encoder's output and the bed share, the cue pre-mute.
void DeckSpatial::encodeStem (Stem& stem, const float* left, const float* right, int count)
{
	float* moving = scratch.getWritePointer (movingRow);
	float* mute = scratch.getWritePointer (muteRow);
	float* cue = scratch.getWritePointer (cueRow);
	const GainSpan movingSpan = fill (stem.moving, moving, count);
	const GainSpan muteSpan = fill (stem.mute, mute, count);
	const GainSpan cueSpan = fill (stem.cue, cue, count);

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

	for (int c = 0; c < busChannels; ++c)
	{
		addScaled (mainSum.getWritePointer (c), stem.io.getReadPointer (c), mute, muteSpan, count);
		addScaled (cueSum.getWritePointer (c), stem.io.getReadPointer (c), cue, cueSpan, count);
	}
	for (int side = 0; side < 2; ++side)
	{
		addScaled (bedIn.getWritePointer (side), steady[side], mute, muteSpan, count);
		addScaled (cueBedIn.getWritePointer (side), steady[side], cue, cueSpan, count);
		addScaled (auxSum.getWritePointer (side), in[side], mute, muteSpan, count);
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
	for (int side = 0; side < 2; ++side)
	{
		float* sum = auxSum.getWritePointer (side);
		FVO::multiply (sum, spatial::trim, count);
		copyIfWanted (out.aux[(size_t) side] == nullptr ? nullptr : out.aux[(size_t) side] + offset, sum, count);
	}
}

// The cued stems in isolation, binaural. Idle, the decoder is not called and
// the cue is 0.0f; after the last cue it runs a hold-over on silence (R12).
void DeckSpatial::renderCue (const Outputs& out, int offset, int count, bool wanted)
{
	if (! cueGate.next (wanted, count))
	{
		cueOut.clear (0, count);
	}
	else
	{
		if (wanted)
		{
			timed (cueBedClock.ticks, [&] { cueBed->process (cueBedIn.getReadPointer (0), cueBedIn.getReadPointer (1), bedOut, count); });
			for (int c = 0; c < busChannels; ++c)
				FVO::add (cueDecoderIo.getWritePointer (c), cueSum.getReadPointer (c), bedOut.getReadPointer (c), count);
		}
		else
		{
			cueDecoderIo.clear (0, count);
		}
		timed (cueDecoderClock.ticks, [&] { runNode (*cueDecoder, cueDecoderIo, count); });
		cueDecoderSamples += count;
		for (int side = 0; side < 2; ++side)
			FVO::multiply (cueOut.getWritePointer (side), cueDecoderIo.getReadPointer (side), spatial::trim, count);
	}
	for (int side = 0; side < 2; ++side)
		copyIfWanted (out.cue[(size_t) side] == nullptr ? nullptr : out.cue[(size_t) side] + offset, cueOut.getReadPointer (side), count);
}

// Standalone phones: the cue's share plus the deck's B-format, binaural. The
// decoder runs only while the phones are rendered and the mix share is above 0.
void DeckSpatial::renderPhones (const Outputs& out, int offset, int count)
{
	const bool connected = out.phones[0] != nullptr && out.phones[1] != nullptr;
	const bool wanted = connected && active (phonesMixWeight);
	float* cueWeight = scratch.getWritePointer (movingRow);
	float* mixWeight = scratch.getWritePointer (muteRow);
	const GainSpan cueSpan = fill (phonesCueWeight, cueWeight, count);
	const GainSpan mixSpan = fill (phonesMixWeight, mixWeight, count);

	const bool decoding = phonesGate.next (wanted, count);
	if (decoding)
	{
		if (wanted)
			for (int c = 0; c < busChannels; ++c)
				FVO::copy (phonesDecoderIo.getWritePointer (c), mainSum.getReadPointer (c), count);
		else
			phonesDecoderIo.clear (0, count);
		timed (phonesDecoderClock.ticks, [&] { runNode (*phonesDecoder, phonesDecoderIo, count); });
		phonesDecoderSamples += count;
	}
	if (! connected)
		return;
	for (int side = 0; side < 2; ++side)
	{
		float* phones = out.phones[(size_t) side] + offset;
		FVO::clear (phones, count);
		addScaled (phones, cueOut.getReadPointer (side), cueWeight, cueSpan, count);
		if (decoding && wanted)
			addScaled (phones, phonesDecoderIo.getReadPointer (side), mixWeight, mixSpan, count);
	}
}

DeckSpatial::Timing DeckSpatial::timing() const
{
	const auto mean = [this] (const Clock& clock)
	{
		if (timedBlocks == 0)
			return 0.0;
		return juce::Time::highResolutionTicksToSeconds (clock.ticks) * 1.0e6 / (double) timedBlocks;
	};
	return { mean (encoderClock), mean (bedClock), mean (cueBedClock), mean (cueDecoderClock), mean (phonesDecoderClock) };
}

DeckSpatial::DecoderRuns DeckSpatial::decoderRuns() const
{
	return { cueDecoderSamples, phonesDecoderSamples };
}

void DeckSpatial::resetTiming()
{
	for (auto* clock : { &encoderClock, &bedClock, &cueBedClock, &cueDecoderClock, &phonesDecoderClock })
		clock->ticks = 0;
	timedBlocks = 0;
	cueDecoderSamples = 0;
	phonesDecoderSamples = 0;
}

float DeckSpatial::movingGainNow (int stem) const
{
	return stemNodes[(size_t) stem]->moving.getCurrentValue();
}
