// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "TwoDeckFixture.h"

// Stage S3: the one-shots move a stem through the deck's audio path. The
// player plays, the ports advance each stem's motion once per block.
using namespace twodeck;

// ORBIT on stem 0, one bar at 120 BPM (2 s), SHIFT: starts at once. Pot 3D
// is 0, so the stem moves only inside the shot's envelope: a quarter round in
// (beat 1, azimuth +90, left) the cue leans left, three quarters in (beat 3,
// azimuth -90, right) it leans right, and after the bar the stem is home.
TEST_F (DeckSpatialMotion, AnOrbitMovesTheCue)
{
	setAllStems (false, false, 1.0f);
	player.setStemCue (0, true);
	player.setBeatGrid ({ 120.0, 0.0 });
	auto& motion = ports.motion();
	ASSERT_TRUE (motion.lengthDown (0));
	ASSERT_TRUE (motion.lengthDown (0));   // 4 -> 2 -> 1 bar
	player.play();
	renderBlocks (blocksFor (0.1));
	const double homeBalance = cueBalanceDb (0.1);

	ASSERT_TRUE (motion.press (0, motionlite::Shape::orbit, true, 1));
	renderBlocks (blocksFor (0.4));                 // to beat 0.8 of the shot
	const double left = cueBalanceDb (0.2);         // beats 0.8 .. 1.2: azimuth ~ +72 .. +108
	EXPECT_EQ (motion.padState (0, motionlite::Shape::orbit), motionlite::PadState::running);
	renderBlocks (blocksFor (0.6));                 // to beat 2.8
	const double right = cueBalanceDb (0.2);        // beats 2.8 .. 3.2: azimuth ~ -108 .. -72

	EXPECT_GT (left - homeBalance, 3.0) << "home " << homeBalance << " dB, left " << left << " dB";
	EXPECT_LT (right - homeBalance, -3.0) << "home " << homeBalance << " dB, right " << right << " dB";

	renderBlocks (blocksFor (0.8));                 // past the bar
	const auto& played = ports.deckSpatial()->controls();
	EXPECT_EQ (motion.padState (0, motionlite::Shape::orbit), motionlite::PadState::idle);
	EXPECT_EQ (played.azimuth[0].load(), ports.settings().azimuth[0].load()) << "the stem is home";
	EXPECT_EQ (played.envelope[0].load(), 0.0f);
	EXPECT_NEAR (cueBalanceDb (0.1), homeBalance, 1.5);
}

// The home is the settings' pose: a stem with no shot plays exactly it, and
// the settings are not touched by a running shot.
TEST_F (DeckSpatialMotion, AShotLeavesTheHomeAlone)
{
	ports.settings().azimuth[1] = -40.0f;
	ports.settings().elevation[1] = 20.0f;
	ports.settings().width[1] = 90.0f;
	ports.settings().threeD[1] = 0.5f;
	player.setBeatGrid ({ 120.0, 0.0 });
	player.play();
	renderBlock();
	const auto& played = ports.deckSpatial()->controls();
	EXPECT_EQ (played.azimuth[1].load(), -40.0f);
	EXPECT_EQ (played.elevation[1].load(), 20.0f);
	EXPECT_EQ (played.width[1].load(), 90.0f);
	EXPECT_EQ (played.threeD[1].load(), 0.5f);

	ASSERT_TRUE (ports.motion().press (1, motionlite::Shape::rise, true, 1));
	renderBlocks (blocksFor (1.0));
	EXPECT_GT (played.elevation[1].load(), 20.0f);
	EXPECT_EQ (ports.settings().elevation[1].load(), 20.0f);
	EXPECT_EQ (ports.motion().posePlayed (1).elevation, played.elevation[1].load());
}
