#include <gtest/gtest.h>

#include "MasterDeck.h"

TEST (MasterDeck, APressedMasterWins)
{
	EXPECT_EQ (chooseMaster (1, { true, true }, 0, true), 1);
}

TEST (MasterDeck, TheMasterIsKept)
{
	EXPECT_EQ (chooseMaster (-1, { true, true }, 0, true), 0);
}

TEST (MasterDeck, TheOnlyPlayingDeckBecomesMaster)
{
	EXPECT_EQ (chooseMaster (-1, { false, true }, -1, true), 1);
}

TEST (MasterDeck, TwoPlayingAndNoneChosenIsNobody)
{
	EXPECT_EQ (chooseMaster (-1, { true, true }, -1, true), -1);
}

TEST (MasterDeck, AStoppedMasterHandsOverToThePlayingDeck)
{
	EXPECT_EQ (chooseMaster (-1, { false, true }, 0, true), 1);
	EXPECT_EQ (chooseMaster (-1, { true, false }, 1, true), 0);
	EXPECT_EQ (chooseMaster (-1, { false, true }, 0, false), 1) << "a master StemDeck holds moves between its decks, under PIO too";
}

TEST (MasterDeck, WithNothingPlayingTheMasterStays)
{
	EXPECT_EQ (chooseMaster (-1, { false, false }, 0, true), 0);
}

// Review 2026-09-29 (I3): MASTER can be turned off, and the automatic master
// only happens where it is allowed -- not under SYNC: PIO (a real CDJ may hold
// master, and two masters flip beat-analyzer's clock) and not after the DJ
// turned MASTER off.
TEST (MasterDeck, PressingTheMasterAgainTurnsItOff)
{
	EXPECT_EQ (chooseMaster (0, { true, false }, 0, true), -1);
}

TEST (MasterDeck, NoAutomaticMasterWhereItIsNotAllowed)
{
	EXPECT_EQ (chooseMaster (-1, { false, true }, -1, false), -1);
	EXPECT_EQ (chooseMaster (1, { false, true }, -1, false), 1) << "a press still works";
}
