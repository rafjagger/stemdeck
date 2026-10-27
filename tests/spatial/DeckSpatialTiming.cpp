// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "SpatialTiming.h"

#include <gtest/gtest.h>

#include <iostream>

TEST (DeckSpatialTiming, FourConfigurations)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto table = sdtest::spatialTiming (2000);
	sdtest::printTable (std::cout, table);
	for (const auto& row : table)
	{
		EXPECT_GT (row.totalMean, 0.0) << row.name;   // a standing deck still clears its ports
		EXPECT_GE (row.totalP99, row.totalMean * 0.5) << row.name;
	}
	// A loose ceiling, so that load cannot flake it: the budget itself is judged on the rig.
	EXPECT_LT (table[3].totalMean, 3000.0);
}

TEST (DeckSpatialTiming, IdleDecodersCostNothing)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto table = sdtest::spatialTiming (500);
	EXPECT_EQ (table[1].nodes.cueDecoder, 0.0);
	EXPECT_EQ (table[1].nodes.phonesDecoder, 0.0);
	EXPECT_GT (table[2].nodes.cueDecoder, 0.0);
	EXPECT_EQ (table[2].nodes.phonesDecoder, 0.0);
	EXPECT_GT (table[3].nodes.phonesDecoder, 0.0);
}

// R14: config (0) runs no node at all.
TEST (DeckSpatialTiming, AStandingDeckRunsNoNode)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto standing = sdtest::spatialTiming (500)[0];
	EXPECT_EQ (standing.nodes.encoders, 0.0);
	EXPECT_EQ (standing.nodes.bed, 0.0);
	EXPECT_EQ (standing.nodes.cueBed, 0.0);
	EXPECT_EQ (standing.nodes.cueDecoder, 0.0);
	EXPECT_EQ (standing.nodes.phonesDecoder, 0.0);
}
