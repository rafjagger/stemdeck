// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <memory>

namespace a3 { class IemNode; }

// REAPER's steady bed for one stereo pair: one IEM MultiEncoder, four sources
// at the azimuths of spatial::bedSources, el 0, -6 dB each, order 3, SN3D.
// It is the same encoder a3-engine's SpatialBus runs on its steady taps.
class SteadyBed
{
public:
	static std::unique_ptr<SteadyBed> create (juce::String& error);
	~SteadyBed();

	void prepare (double sampleRate, int blockSize);   // allocates
	// Overwrites the first numSamples of bformat16's 16 channels; numSamples <= the prepared block.
	void process (const float* left, const float* right, juce::AudioBuffer<float>& bformat16, int numSamples);

private:
	explicit SteadyBed (std::unique_ptr<a3::IemNode>);
	std::unique_ptr<a3::IemNode> encoder;
};
