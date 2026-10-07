#include <gtest/gtest.h>

#include "Outputs.h"

// StemDeck's one output mode since spec stemdeck-remote (2026-10-01): the five
// stereo buses, switched by remote control from the desk. The 8x stereo
// mode (every stem on its own pair) is gone, and since 2026-10-07 so are the
// PHONES outputs: the cue is the desk channel, in REAPER.

TEST (Outputs, FiveStereoBusesAreTheOnlyOutput)
{
	EXPECT_EQ (outputs::channelCount(), 10);
	EXPECT_EQ (outputs::portName (0), "deck1_L");
	EXPECT_EQ (outputs::portName (9), "aux_R");
}

// The names as registered on the rig and carried by zita-j2n's ten channels
// (start.sh: deck1_L ... aux_R).
TEST (Outputs, TheBusPortsKeepTodaysNames)
{
	const char* expected[] = { "deck1_L", "deck1_R", "deck2_L", "deck2_R", "deck3_L", "deck3_R",
							   "deck4_L", "deck4_R", "aux_L", "aux_R" };
	ASSERT_EQ (outputs::channelCount(), (int) std::size (expected));
	for (int ch = 0; ch < outputs::channelCount(); ++ch)
		EXPECT_EQ (outputs::portName (ch), expected[ch]);
}

TEST (Outputs, NoPhonesPorts)
{
	for (int ch = 0; ch < outputs::channelCount(); ++ch)
		EXPECT_EQ (outputs::portName (ch).find ("phones"), std::string::npos) << outputs::portName (ch);
}
