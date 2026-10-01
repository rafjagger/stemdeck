#include <gtest/gtest.h>

#include "Outputs.h"

// StemDeck's one output mode since spec stemdeck-remote (2026-10-01): the six
// stereo buses, switched by remote control from the desk. The 8x stereo
// mode (every stem on its own pair) is gone.

TEST (Outputs, SixStereoBusesAreTheOnlyOutput)
{
	EXPECT_EQ (outputs::channelCount(), 12);
	EXPECT_EQ (outputs::portName (0), "deck1_L");
	EXPECT_EQ (outputs::portName (11), "phones_R");
}

// Today's names, as registered on the rig (start.sh: deck1_L ... phones_R).
TEST (Outputs, TheBusPortsKeepTodaysNames)
{
	const char* expected[] = { "deck1_L", "deck1_R", "deck2_L", "deck2_R", "deck3_L", "deck3_R",
							   "deck4_L", "deck4_R", "aux_L", "aux_R", "phones_L", "phones_R" };
	for (int ch = 0; ch < 12; ++ch)
		EXPECT_EQ (outputs::portName (ch), expected[ch]);
}
