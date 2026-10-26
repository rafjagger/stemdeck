// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "DeckSpatialFixture.h"

#include <SpatialConfig.h>

using namespace sdtest;

namespace
{
a3::SpatialConfig enginesConfig()
{
	juce::String error;
	auto config = a3::loadSpatialConfig (juce::File (STEMDECK_SPATIAL_DATA), error);
	if (! config)
		throw std::runtime_error (error.toStdString());
	return *config;
}

// a3-engine's encoder, as its SpatialBus builds it, fed only one strip's
// steady taps: sources 4-7 by encoder.inputs.json, the moving sources silent.
std::array<Signal, spatial::bformatChannels> enginesBed (const Signal& left, const Signal& right)
{
	const auto config = enginesConfig();
	auto encoder = a3::IemNode::create (a3::IemKind::multiEncoder, a3::numEncoderSources, spatial::bformatChannels);
	if (encoder == nullptr)
		throw std::runtime_error ("no MultiEncoder");
	const auto set = encoder->setParameters (config.encoderParameters);
	if (set.failed())
		throw std::runtime_error (set.getErrorMessage().toStdString());
	encoder->prepare (rate, block);

	const int frames = (int) left.size();
	const Signal* sides[2] = { &left, &right };
	juce::AudioBuffer<float> all (spatial::bformatChannels, frames), io (spatial::bformatChannels, block);
	for (int offset = 0; offset < frames; offset += block)
	{
		const int count = std::min (block, frames - offset);
		juce::AudioBuffer<float> view (io.getArrayOfWritePointers(), spatial::bformatChannels, count);
		view.clear();
		for (int source = 0; source < a3::numEncoderSources; ++source)
			for (const auto& entry : config.sources[(size_t) source])
				if (entry.tap == a3::Tap::steady && entry.strip == 0)
					view.addFrom (source, 0, sides[entry.side]->data() + offset, count, entry.gain);
		encoder->process (view);
		for (int c = 0; c < spatial::bformatChannels; ++c)
			all.copyFrom (c, offset, io, c, 0, count);
	}
	return scaled (all, frames, 1.0f);
}
}

TEST (SteadyBed, MatchesTheEnginesBed)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto left = noise (oneSecond, 1), right = noise (oneSecond, 2);
	const auto reference = enginesBed (left, right);
	const auto bed = bedReference (left, right);   // x trim
	for (int c = 0; c < spatial::bformatChannels; ++c)
	{
		Signal untrimmed (bed[(size_t) c]);
		for (auto& sample : untrimmed)
			sample /= spatial::trim;
		EXPECT_LE (maxDifference (untrimmed, reference[(size_t) c]), 1e-5f) << "channel " << c;
	}
	EXPECT_GT (rmsDb (reference[0]), -30.0) << "the reference carries signal";
}
