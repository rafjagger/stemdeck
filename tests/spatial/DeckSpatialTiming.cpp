// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "SpatialTiming.h"

#include <gtest/gtest.h>

#include <iostream>

TEST (DeckSpatialTiming, ThreeConfigurations)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto table = sdtest::spatialTiming (2000);
	sdtest::printTable (std::cout, table);
	for (const auto& row : table)
	{
		EXPECT_GT (row.totalMean, 0.0) << row.name;
		EXPECT_GE (row.totalP99, row.totalMean * 0.5) << row.name;
	}
	// A loose ceiling, so that load cannot flake it: the budget itself is judged on the rig.
	EXPECT_LT (table[2].totalMean, 3000.0);
}

TEST (DeckSpatialTiming, IdleDecodersCostNothing)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto table = sdtest::spatialTiming (500);
	EXPECT_EQ (table[0].nodes.cueDecoder, 0.0);
	EXPECT_EQ (table[0].nodes.phonesDecoder, 0.0);
	EXPECT_GT (table[1].nodes.cueDecoder, 0.0);
	EXPECT_EQ (table[1].nodes.phonesDecoder, 0.0);
	EXPECT_GT (table[2].nodes.phonesDecoder, 0.0);
}
