// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "SpatialTiming.h"

#include <gtest/gtest.h>

#include <iostream>

TEST (DeckSpatialTiming, OneStemAndFourStems)
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

// The rig's configurations decode nothing; only the standalone phones do.
TEST (DeckSpatialTiming, OnlyThePhonesDecode)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto table = sdtest::spatialTiming (500);
	for (const auto rig : { 1, 2, 4 })
		EXPECT_EQ (table[(size_t) rig].nodes.phonesDecoder, 0.0) << table[(size_t) rig].name;
	EXPECT_GT (table[2].nodes.cueBed, 0.0);
	EXPECT_GT (table[3].nodes.phonesDecoder, 0.0);
	EXPECT_GT (table[5].nodes.phonesDecoder, 0.0);
}

// R14: config (0) runs no node at all.
TEST (DeckSpatialTiming, AStandingDeckRunsNoNode)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto standing = sdtest::spatialTiming (500)[0];
	EXPECT_EQ (standing.nodes.encoders, 0.0);
	EXPECT_EQ (standing.nodes.bed, 0.0);
	EXPECT_EQ (standing.nodes.cueBed, 0.0);
	EXPECT_EQ (standing.nodes.phonesDecoder, 0.0);
}
