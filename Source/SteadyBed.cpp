// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "SteadyBed.h"

#include "SpatialLaw.h"
#include "a3-engine/IemNode.h"

namespace
{
constexpr float iemOrderThree = 4.0f;   // IEM's choice value for 3rd order (0 = auto)
constexpr int sources = (int) spatial::bedSources.size();

// The bed's parameters, set before prepare so the warm-up block settles IEM's
// coefficient ramp and the first real block is already in place.
juce::Result configure (a3::IemNode& encoder)
{
	auto* parameters = new juce::DynamicObject();
	parameters->setProperty ("inputSetting", sources);
	parameters->setProperty ("orderSetting", iemOrderThree);
	parameters->setProperty ("useSN3D", 1.0f);
	for (int i = 0; i < sources; ++i)
	{
		const auto suffix = juce::String (i);
		parameters->setProperty ("azimuth" + suffix, spatial::bedSources[(size_t) i].azimuth);
		parameters->setProperty ("elevation" + suffix, 0.0f);
		parameters->setProperty ("gain" + suffix, spatial::bedSourceGainDb);
	}
	return encoder.setParameters (juce::var (parameters));
}
}

std::unique_ptr<SteadyBed> SteadyBed::create (juce::String& error)
{
	auto encoder = a3::IemNode::create (a3::IemKind::multiEncoder, sources, spatial::bformatChannels);
	if (encoder == nullptr)
	{
		error = "steady bed: IEM refused a MultiEncoder with " + juce::String (sources) + " inputs";
		return nullptr;
	}
	const auto configured = configure (*encoder);
	if (configured.failed())
	{
		error = "steady bed: " + configured.getErrorMessage();
		return nullptr;
	}
	return std::unique_ptr<SteadyBed> (new SteadyBed (std::move (encoder)));
}

SteadyBed::SteadyBed (std::unique_ptr<a3::IemNode> node) : encoder (std::move (node)) {}

SteadyBed::~SteadyBed() = default;

void SteadyBed::prepare (double sampleRate, int blockSize)
{
	encoder->prepare (sampleRate, blockSize);
}

void SteadyBed::process (const float* left, const float* right, juce::AudioBuffer<float>& bformat16, int numSamples)
{
	jassert (bformat16.getNumChannels() >= spatial::bformatChannels && numSamples <= bformat16.getNumSamples());
	const float* const sides[2] = { left, right };
	for (int i = 0; i < sources; ++i)
		bformat16.copyFrom (i, 0, sides[spatial::bedSources[(size_t) i].side], numSamples);
	// A view of numSamples: JUCE's preallocated channel table, no allocation.
	juce::AudioBuffer<float> chunk (bformat16.getArrayOfWritePointers(), spatial::bformatChannels, numSamples);
	encoder->process (chunk);
}
