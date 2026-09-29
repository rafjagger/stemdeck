#include <gtest/gtest.h>

#include "MasterDeck.h"

TEST (MasterDeck, APressedMasterWins)
{
	EXPECT_EQ (chooseMaster (1, { true, true }, 0), 1);
}

TEST (MasterDeck, TheMasterIsKept)
{
	EXPECT_EQ (chooseMaster (-1, { true, true }, 0), 0);
}

TEST (MasterDeck, TheOnlyPlayingDeckBecomesMaster)
{
	EXPECT_EQ (chooseMaster (-1, { false, true }, -1), 1);
}

TEST (MasterDeck, TwoPlayingAndNoneChosenIsNobody)
{
	EXPECT_EQ (chooseMaster (-1, { true, true }, -1), -1);
}

TEST (MasterDeck, AStoppedMasterStaysMaster)
{
	EXPECT_EQ (chooseMaster (-1, { false, true }, 0), 0);
}
