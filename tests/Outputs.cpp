#include <gtest/gtest.h>

#include "Outputs.h"

// StemDeck's two output modes (spec stem-routing-on-the-desk): today's six
// stereo buses, or every stem on its own pair for the desk to mix.

TEST (Outputs, SixteenOutsForTheStems)
{
	EXPECT_EQ (outputs::channelCount (outputs::Mode::Stems), 16);
	EXPECT_EQ (outputs::channelCount (outputs::Mode::Buses), 12);
}

TEST (Outputs, DeckAThenDeckB)
{
	EXPECT_EQ (outputs::stemChannel (0, 0, 0), 0);
	EXPECT_EQ (outputs::stemChannel (0, 3, 1), 7);
	EXPECT_EQ (outputs::stemChannel (1, 0, 0), 8);
	EXPECT_EQ (outputs::stemChannel (1, 3, 1), 15);
}

TEST (Outputs, PortsSayWhatTheyCarry)
{
	EXPECT_EQ (outputs::portName (outputs::Mode::Stems, 0), "a1_L");
	EXPECT_EQ (outputs::portName (outputs::Mode::Stems, 15), "b4_R");
	EXPECT_EQ (outputs::portName (outputs::Mode::Buses, 0), "deck1_L");
	EXPECT_EQ (outputs::portName (outputs::Mode::Buses, 11), "phones_R");
}

// Today's names, as registered on the rig (start.sh: deck1_L ... phones_R).
TEST (Outputs, TheBusPortsKeepTodaysNames)
{
	const char* expected[] = { "deck1_L", "deck1_R", "deck2_L", "deck2_R", "deck3_L", "deck3_R",
							   "deck4_L", "deck4_R", "aux_L", "aux_R", "phones_L", "phones_R" };
	for (int ch = 0; ch < 12; ++ch)
		EXPECT_EQ (outputs::portName (outputs::Mode::Buses, ch), expected[ch]);
}

TEST (Outputs, EveryStemPortMatchesItsChannel)
{
	for (int deck = 0; deck < 2; ++deck)
		for (int stem = 0; stem < 4; ++stem)
			for (int side = 0; side < 2; ++side)
				EXPECT_EQ (outputs::portName (outputs::Mode::Stems, outputs::stemChannel (deck, stem, side)),
						   std::string (deck == 0 ? "a" : "b") + std::to_string (stem + 1) + (side == 0 ? "_L" : "_R"));
}

TEST (Outputs, AnUnknownSettingIsTheBuses)
{
	EXPECT_EQ (outputs::modeFromSetting ("stems"), outputs::Mode::Stems);
	EXPECT_EQ (outputs::modeFromSetting (""), outputs::Mode::Buses);
	EXPECT_EQ (outputs::modeFromSetting ("x"), outputs::Mode::Buses);
}

TEST (Outputs, TheSettingRoundTrips)
{
	for (auto mode : { outputs::Mode::Buses, outputs::Mode::Stems })
		EXPECT_EQ (outputs::modeFromSetting (outputs::settingValue (mode)), mode);
}

TEST (Outputs, TheStatusLineNamesTheMode)
{
	EXPECT_EQ (outputs::statusLabel (outputs::Mode::Stems), "8\xc3\x97 ST");
	EXPECT_EQ (outputs::statusLabel (outputs::Mode::Buses), "6\xc3\x97 ST");
}
