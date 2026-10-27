#include <gtest/gtest.h>

#include "Outputs.h"

#include <set>
#include <string>
#include <vector>

// StemDeck's one output mode since spec stemdeck-remote (2026-10-01): the five
// stereo buses, switched by remote control from the desk. The 8x stereo
// mode (every stem on its own pair) is gone, and since 2026-10-07 so are the
// PHONES outputs: the cue is the desk channel, in REAPER.

TEST (Outputs, FiveStereoBusesThenTheCuePair)
{
	EXPECT_EQ (outputs::busChannelCount(), 10);
	EXPECT_EQ (outputs::stereo::channelCount(), 12);
	EXPECT_EQ (outputs::portName (0), "deck1_L");
	EXPECT_EQ (outputs::portName (9), "aux_R");
}

// The names as registered on the rig and carried by the ten channels to the Core
// (start.sh: deck1_L ... aux_R).
TEST (Outputs, TheBusPortsKeepTodaysNames)
{
	const char* expected[] = { "deck1_L", "deck1_R", "deck2_L", "deck2_R", "deck3_L", "deck3_R",
							   "deck4_L", "deck4_R", "aux_L", "aux_R" };
	ASSERT_EQ (outputs::busChannelCount(), (int) std::size (expected));
	for (int ch = 0; ch < outputs::busChannelCount(); ++ch)
		EXPECT_EQ (outputs::portName (ch), expected[ch]);
}

// Appended after the buses, so no existing port moves.
TEST (Outputs, TheCuePairComesLast)
{
	EXPECT_EQ (outputs::portName (10), "cue_L");
	EXPECT_EQ (outputs::portName (11), "cue_R");
	EXPECT_EQ (outputs::cueChannel(), 10);
}

// Today's twelve carry no phones port; the spatial ports (R7) come after them.
TEST (Outputs, NoPhonesPorts)
{
	for (int ch = 0; ch < outputs::stereo::channelCount(); ++ch)
		EXPECT_EQ (outputs::portName (ch).find ("phones"), std::string::npos) << outputs::portName (ch);
}

// Stage S2 (ruling R7): after today's twelve, each deck adds its B-format, a
// binaural cue and phones -- 20 channels, deck A first.
TEST (Outputs, TodaysTwelvePortsKeepTheirNamesAndOrder)
{
	const char* expected[] = { "deck1_L", "deck1_R", "deck2_L", "deck2_R", "deck3_L", "deck3_R",
							   "deck4_L", "deck4_R", "aux_L", "aux_R", "cue_L", "cue_R" };
	ASSERT_EQ (outputs::stereo::channelCount(), (int) std::size (expected));
	for (int ch = 0; ch < outputs::stereo::channelCount(); ++ch)
		EXPECT_EQ (outputs::portName (ch), expected[ch]);
}

TEST (Outputs, EachDeckAddsTwentySpatialPorts)
{
	EXPECT_EQ (outputs::spatialFirstChannel (0), 12);
	EXPECT_EQ (outputs::spatialFirstChannel (1), 32);
	EXPECT_EQ (outputs::portName (12), "A_bformat_1");
	EXPECT_EQ (outputs::portName (27), "A_bformat_16");
	EXPECT_EQ (outputs::portName (28), "A_bcue_L");
	EXPECT_EQ (outputs::portName (29), "A_bcue_R");
	EXPECT_EQ (outputs::portName (30), "A_phones_L");
	EXPECT_EQ (outputs::portName (31), "A_phones_R");
	EXPECT_EQ (outputs::portName (32), "B_bformat_1");
	EXPECT_EQ (outputs::portName (47), "B_bformat_16");
	EXPECT_EQ (outputs::portName (48), "B_bcue_L");
	EXPECT_EQ (outputs::portName (51), "B_phones_R");
	EXPECT_EQ (outputs::channelCount(), 52);
}

TEST (Outputs, EveryPortNameIsUnique)
{
	std::set<std::string> names;
	for (int ch = 0; ch < outputs::channelCount(); ++ch)
		EXPECT_TRUE (names.insert (outputs::portName (ch)).second) << outputs::portName (ch);
}

// Without JACK: the buses are summed down onto fewer device outputs, as
// today; the cue and every spatial channel go only to an output of their own.
TEST (Outputs, SpatialChannelsAreNeverSummedDown)
{
	EXPECT_EQ (outputs::foldsInto (0, 2), 0);
	EXPECT_EQ (outputs::foldsInto (3, 2), 1);
	EXPECT_EQ (outputs::foldsInto (9, 2), 1);
	EXPECT_EQ (outputs::foldsInto (9, 12), 9);
	for (int ch = outputs::cueChannel(); ch < outputs::channelCount(); ++ch)
	{
		EXPECT_EQ (outputs::foldsInto (ch, 2), -1) << ch;
		EXPECT_EQ (outputs::foldsInto (ch, ch), -1) << ch;
		EXPECT_EQ (outputs::foldsInto (ch, ch + 1), ch) << ch;
		EXPECT_EQ (outputs::foldsInto (ch, 64), ch) << ch;
	}
	EXPECT_EQ (outputs::foldsInto (0, 0), -1);
}

// A -bformat instance (spec section 8): its own JACK client with 22 outputs,
// the deck's B-format in ACN order, its binaural cue, its AUX and its phones
// -- none of today's bus ports.
TEST (Outputs, OneDeckPortsAreSDsSection8)
{
	std::vector<std::string> expected;
	for (int c = 1; c <= 16; ++c)
		expected.push_back ("bformat_" + std::to_string (c));
	for (const auto* pair : { "cue", "aux", "phones" })
		for (const auto* side : { "_L", "_R" })
			expected.push_back (std::string (pair) + side);

	ASSERT_EQ (outputs::oneDeck::channelCount(), 22);
	for (int ch = 0; ch < outputs::oneDeck::channelCount(); ++ch)
		EXPECT_EQ (outputs::oneDeck::portName (ch), expected[(size_t) ch]) << ch;

	const auto layout = outputs::oneDeck::layout();
	EXPECT_EQ (layout.channels, 22);
	EXPECT_EQ (outputs::oneDeck::portName (layout.bformat), "bformat_1");
	EXPECT_EQ (outputs::oneDeck::portName (layout.cue), "cue_L");
	EXPECT_EQ (outputs::oneDeck::portName (layout.aux), "aux_L");
	EXPECT_EQ (outputs::oneDeck::portName (layout.phones), "phones_L");
}

// Two-deck mode keeps its 20 spatial channels per deck and renders no AUX
// there: the AUX bus stays the AUX.
TEST (Outputs, TwoDeckSpatialLayoutIsUnchanged)
{
	const auto layout = outputs::twoDeckSpatialLayout();
	EXPECT_EQ (layout.channels, outputs::spatialChannelsPerDeck());
	EXPECT_EQ (layout.bformat, outputs::bformatOffset);
	EXPECT_EQ (layout.cue, outputs::binauralCueOffset);
	EXPECT_EQ (layout.aux, -1);
	EXPECT_EQ (layout.phones, outputs::phonesOffset);
}

// Without JACK: on a device with all 22 outputs each channel has its own; on
// a smaller one (a laptop's stereo) only the phones play, on its first pair --
// a B-format summed into stereo is no mix anyone could listen to.
TEST (Outputs, OneDeckWithoutJackOnlyThePhonesFoldOntoStereo)
{
	using outputs::oneDeck::foldsInto;
	const auto phones = outputs::oneDeck::layout().phones;
	for (int ch = 0; ch < 22; ++ch)
		EXPECT_EQ (foldsInto (ch, 22), ch) << ch;
	EXPECT_EQ (foldsInto (phones, 2), 0);
	EXPECT_EQ (foldsInto (phones + 1, 2), 1);
	for (int ch = 0; ch < phones; ++ch)
		EXPECT_EQ (foldsInto (ch, 2), -1) << ch;
	EXPECT_EQ (foldsInto (phones, 1), -1);
	EXPECT_EQ (foldsInto (phones, 0), -1);
}
