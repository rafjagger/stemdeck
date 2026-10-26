// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "StemKnob.h"
#include "TwoDeckFixture.h"

using namespace twodeck;

// R4: a stem knob at its bottom is exact silence everywhere a stem goes --
// the pre-mute block, the B-format, the binaural cue and the phones -- even
// with every stem on cue, once the knob's ramp has settled.
TEST_F (TwoDeckSpatial, StemGainMinimumIsExactSilenceOnEverySpatialPort)
{
	setAllStems (false, true, 1.0f);
	player.play();
	renderBlocks (blocksFor (0.3));
	ASSERT_GT (spatialPeak (bformatFirst), 0.0f) << "the deck plays into its B-format";
	ASSERT_GT (spatialPeak (cueFirst), 0.0f) << "and into its cue";

	for (int s = 0; s < stems; ++s)
		player.setStemGain (s, juce::Decibels::decibelsToGain (stemKnobMinimumDb, stemKnobMinimumDb));
	renderBlocks (blocksFor (0.5));

	for (int i = 0; i < blocksFor (0.2); ++i)
	{
		renderBlock();
		ASSERT_EQ (player.getPreMuteBlockSamples(), block);
		for (int ch = 0; ch < StemDeckPlayer::numOutputChannels; ++ch)
			ASSERT_TRUE (exactlySilent (player.getPreMuteBlock().getReadPointer (ch), block)) << "pre-mute channel " << ch;
		for (int ch = 0; ch < outputs::spatialChannelsPerDeck(); ++ch)
			ASSERT_TRUE (exactlySilent (spatialChannel (ch), block)) << outputs::portName (outputs::spatialFirstChannel (0) + ch);
	}
}

// The pre-mute block is the stem after its knob, before its mute: muted, a
// stem still fills it, and with its cue switch settled it is the cue send.
TEST_F (TwoDeckSpatial, ThePreMuteBlockIsTheStemAfterItsKnobBeforeItsMute)
{
	setAllStems (true, true, 0.5f);
	player.play();
	renderBlocks (blocksFor (0.1));

	ASSERT_EQ (player.getPreMuteBlockSamples(), block);
	ASSERT_EQ (player.getCueBlockSamples(), block);
	for (int ch = 0; ch < StemDeckPlayer::numOutputChannels; ++ch)
	{
		EXPECT_GT (player.getPreMuteBlock().getMagnitude (ch, 0, block), 0.0f) << ch;
		EXPECT_EQ (deck.getMagnitude (ch, 0, block), 0.0f) << "muted: the stem output is silent, channel " << ch;
		for (int i = 0; i < block; ++i)
			ASSERT_EQ (player.getPreMuteBlock().getSample (ch, i), player.getCueBlock().getSample (ch, i)) << ch << "/" << i;
	}
}

// MUTE leaves the deck's B-format exactly silent but not its cue (R5), and
// in two-deck mode the phones pair carries the cue (level 1, mix 0).
TEST_F (TwoDeckSpatial, MuteSilencesTheBformatButNotTheCue)
{
	setAllStems (true, true, 1.0f);
	player.play();
	renderBlocks (blocksFor (0.2));

	for (int c = 0; c < 16; ++c)
		EXPECT_TRUE (exactlySilent (spatialChannel (bformatFirst + c), block)) << "bformat " << c;
	for (int side = 0; side < 2; ++side)
	{
		EXPECT_GT (spatialPeak (cueFirst + side), 0.0f) << "cue " << side;
		for (int i = 0; i < block; ++i)
			ASSERT_EQ (spatialChannel (phonesFirst + side)[i], spatialChannel (cueFirst + side)[i]) << side << "/" << i;
	}
}

// Unmuted and off cue: B-format carries the deck, the cue and phones are silent.
TEST_F (TwoDeckSpatial, OffCueTheDeckGoesToTheBformatOnly)
{
	setAllStems (false, false, 1.0f);
	player.play();
	renderBlocks (blocksFor (0.2));

	EXPECT_GT (spatialPeak (bformatFirst), 0.0f);
	for (int side = 0; side < 2; ++side)
	{
		EXPECT_TRUE (exactlySilent (spatialChannel (cueFirst + side), block)) << "cue " << side;
		EXPECT_TRUE (exactlySilent (spatialChannel (phonesFirst + side), block)) << "phones " << side;
	}
}

// A deck that stands gives no pre-mute block and exactly silent spatial ports.
TEST_F (TwoDeckSpatial, AStandingDeckIsExactSilence)
{
	setAllStems (false, true, 1.0f);
	renderBlocks (blocksFor (0.1));

	EXPECT_EQ (player.getPreMuteBlockSamples(), 0);
	for (int ch = 0; ch < outputs::spatialChannelsPerDeck(); ++ch)
		EXPECT_TRUE (exactlySilent (spatialChannel (ch), block)) << ch;
}

// The ports write deck A's 20 channels and nothing else: today's twelve and
// deck B's stay as they were.
TEST_F (TwoDeckSpatial, WritesOnlyItsOwnTwentyChannels)
{
	setAllStems (false, true, 1.0f);
	player.play();
	renderBlocks (blocksFor (0.1));

	for (int ch = 0; ch < outputs::channelCount(); ++ch)
		juce::FloatVectorOperations::fill (bus.getWritePointer (ch), 0.5f, block);
	renderBlock();

	const auto first = outputs::spatialFirstChannel (0);
	for (int ch = 0; ch < outputs::channelCount(); ++ch)
	{
		if (ch >= first && ch < first + outputs::spatialChannelsPerDeck())
			continue;
		for (int i = 0; i < block; ++i)
			ASSERT_EQ (bus.getSample (ch, i), 0.5f) << outputs::portName (ch);
	}
}

// Two-deck mode: deck 3D 1, phones = cue, StemDeck's own phones decoder off.
TEST_F (TwoDeckSpatial, TwoDeckControlsAreFixed)
{
	auto* spatial = ports.deckSpatial();
	ASSERT_NE (spatial, nullptr);
	auto& c = spatial->controls();
	EXPECT_EQ (c.deckThreeD.load(), 1.0f);
	EXPECT_EQ (c.phonesLevel.load(), 1.0f);
	EXPECT_EQ (c.phonesMix.load(), 0.0f);
	EXPECT_FALSE (c.phonesDecoderOn.load());

	setAllStems (false, false, 1.0f);
	player.setStemMuted (2, true);
	player.setStemCue (1, true);
	player.play();
	renderBlock();
	for (int s = 0; s < stems; ++s)
	{
		EXPECT_EQ (c.muted[(size_t) s].load(), s == 2) << s;
		EXPECT_EQ (c.cued[(size_t) s].load(), s == 1) << s;
	}
}

// R14: the spatial deck stands while the player does, and plays when it plays.
TEST_F (TwoDeckSpatial, TheSpatialDeckFollowsThePlayersTransport)
{
	auto& c = ports.deckSpatial()->controls();
	setAllStems (false, true, 1.0f);
	renderBlock();
	EXPECT_FALSE (c.playing.load());

	player.play();
	renderBlock();
	EXPECT_TRUE (c.playing.load());

	player.stop();
	renderBlocks (blocksFor (0.1));
	ports.deckSpatial()->resetTiming();
	renderBlocks (blocksFor (0.1));
	EXPECT_FALSE (c.playing.load());
	EXPECT_EQ (ports.deckSpatial()->timing().encoders, 0.0) << "a stopped deck stands";
}
