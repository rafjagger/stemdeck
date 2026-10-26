// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "DecoderGate.h"
#include "SpatialLaw.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

namespace a3 { class IemNode; struct StereoPose; }
class SteadyBed;

// One deck's spatial engine (design spec §2, §5-§7): per stem a StereoEncoder
// fed the moving share g·x and the steady share (1-g)·x into the main bed, so
// the deck leaves as 16-channel B-format; the cued stems through a cue bed and
// a BinauralDecoder; AUX as the plain stereo sum; phones for standalone use.
// The encoders are fed pre-MUTE and MUTE applies to their output (ruling R5),
// so the cue stays pre-mute. Every gain ramps over 10 ms; every volume at its
// minimum is exact silence (R4). The decoders run only while used (R12).
class DeckSpatial
{
public:
	static constexpr int stems = spatial::stemsPerDeck;

	struct Controls   // written from any thread, read once per block
	{
		Controls();
		std::array<std::atomic<float>, stems> threeD {}, azimuth {}, elevation {}, width {};   // width default 60
		std::array<std::atomic<bool>, stems> muted {}, cued {};
		std::atomic<float> deckThreeD { 1.0f }, phonesLevel { 1.0f }, phonesMix { 0.0f };
		std::atomic<bool> phonesDecoderOn { false };   // the app sets it from spatial::phonesDecoderRuns
		std::array<std::atomic<float>, stems> envelope {};   // a one-shot's 3D envelope; 0 = none
	};

	// A null entry: that output is not rendered.
	struct Outputs
	{
		std::array<float*, spatial::bformatChannels> bformat {};
		std::array<float*, 2> cue {}, aux {}, phones {};
	};

	// Mean µs per process() call since resetTiming(); 0 for a node never called. Audio thread only.
	struct Timing { double encoders, bed, cueBed, cueDecoder, phonesDecoder; };
	// Samples each decoder ran since resetTiming(). Audio thread only.
	struct DecoderRuns { std::int64_t cue, phones; };

	static std::unique_ptr<DeckSpatial> create (juce::String& error);
	~DeckSpatial();

	void prepare (double sampleRate, int blockSize);   // allocates; IEM nodes at blockSize, longer calls chunked
	// stemsPreMute: 8 channels, stem s at 2s and 2s+1, post GAIN, pre MUTE.
	void process (const float* const* stemsPreMute, const Outputs&, int numSamples);
	Controls& controls();

	Timing timing() const;
	DecoderRuns decoderRuns() const;
	void resetTiming();
	float movingGainNow (int stem) const;   // the encoder input gain where the ramp is now

private:
	struct Stem;
	using Ramp = juce::LinearSmoothedValue<float>;
	struct Clock { std::int64_t ticks = 0; };

	DeckSpatial();
	void readControls();
	void processChunk (const float* const* stemsPreMute, const Outputs&, int offset, int count);
	void encodeStem (Stem&, const float* left, const float* right, int count);
	a3::StereoPose poseOf (int stem) const;
	void renderMain (const Outputs&, int offset, int count);
	void renderCue (const Outputs&, int offset, int count, bool wanted);
	void renderPhones (const Outputs&, int offset, int count);
	bool cueWanted() const;

	Controls state;
	std::array<std::unique_ptr<Stem>, stems> stemNodes;
	std::unique_ptr<SteadyBed> mainBed, cueBed;
	std::unique_ptr<a3::IemNode> cueDecoder, phonesDecoder;
	DecoderGate cueGate, phonesGate;
	Ramp phonesCueWeight, phonesMixWeight;

	// Prepared at blockSize: 16-ch sums and decoder io, stereo bed inputs, per-sample gains.
	juce::AudioBuffer<float> mainSum, cueSum, bedOut, cueDecoderIo, phonesDecoderIo;
	juce::AudioBuffer<float> bedIn, cueBedIn, auxSum, cueOut, scratch;
	int blockSize = 0;

	Clock encoderClock, bedClock, cueBedClock, cueDecoderClock, phonesDecoderClock;
	std::int64_t timedBlocks = 0, cueDecoderSamples = 0, phonesDecoderSamples = 0;
};
