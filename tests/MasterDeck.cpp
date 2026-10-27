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

// S4.7, one deck per instance (-bformat): MASTER across instances on the link.
// The newest claim wins; an instance publishes only while it is master.
namespace
{
	constexpr int other = 8;
}

TEST (MasterDeck, OneDeckInstancePublishesOnlyWhenMaster)
{
	InstanceMaster master;
	EXPECT_FALSE (master.isMaster()) << "nothing goes out before a press";
	master.press (10.0);
	EXPECT_TRUE (master.isMaster());
	master.press (11.0);
	EXPECT_FALSE (master.isMaster()) << "pressed on the master: off";
}

TEST (MasterDeck, ANewerClaimTakesMasterAway)
{
	InstanceMaster master;
	master.joined (5.0);
	master.press (10.0);
	master.onStatus (other, true, 12.0);
	EXPECT_TRUE (master.yieldIfOvertaken (12.0));
	EXPECT_FALSE (master.isMaster());
	EXPECT_FALSE (master.yieldIfOvertaken (12.2)) << "it yields once";
}

TEST (MasterDeck, AnOlderClaimDoesNotTakeMasterAway)
{
	InstanceMaster master;
	master.joined (5.0);
	for (double t = 6.0; t < 10.3; t += 0.2)   // the other deck was master first: a status every 200 ms
		master.onStatus (other, true, t);
	master.press (10.0);
	EXPECT_FALSE (master.yieldIfOvertaken (10.2));
	EXPECT_TRUE (master.isMaster());
}

TEST (MasterDeck, AClaimHeardOnJoiningIsOlderThanThePress)
{
	// MASTER pressed with the link off: it joins at the press, and the other
	// deck's running claim is first heard just after -- it is still the older one.
	InstanceMaster master;
	master.joined (10.0);
	master.press (10.0);
	master.onStatus (other, true, 10.15);
	EXPECT_FALSE (master.yieldIfOvertaken (10.15));
	EXPECT_TRUE (master.isMaster());
}

TEST (MasterDeck, AClaimThatStoppedAndCameBackIsNew)
{
	InstanceMaster master;
	master.joined (5.0);
	master.onStatus (other, true, 9.8);
	master.press (10.0);
	master.onStatus (other, false, 10.5);   // it gave master up ...
	master.onStatus (other, true, 12.0);    // ... and took it again after this press
	EXPECT_TRUE (master.yieldIfOvertaken (12.0));
}

TEST (MasterDeck, ASilentClaimIsForgotten)
{
	InstanceMaster master;
	master.joined (5.0);
	master.press (6.0);
	master.onStatus (other, true, 7.0);
	ASSERT_TRUE (master.yieldIfOvertaken (7.0));
	master.press (8.0);
	EXPECT_FALSE (master.yieldIfOvertaken (8.0)) << "the same claim, older than the new press";
	EXPECT_FALSE (master.yieldIfOvertaken (12.0)) << "silent for two seconds: gone";
	EXPECT_TRUE (master.isMaster());
}

// Fix round 1: MASTER pressed while the link could not start yet; it joins on
// a retry two seconds later. The running master's claim, heard on joining, is
// still older than the press: this deck keeps master, and the old master --
// listening all along -- hears this deck's claim as new and yields.
TEST (MasterDeck, AJoinAfterThePressKeepsTheNewMaster)
{
	constexpr int here = 6;
	InstanceMaster pressed, old;
	old.joined (1.0);
	old.press (2.0);

	pressed.press (10.0);
	pressed.joined (12.0);
	pressed.onStatus (other, true, 12.1);
	EXPECT_FALSE (pressed.yieldIfOvertaken (12.1));
	EXPECT_TRUE (pressed.isMaster());

	old.onStatus (here, true, 12.2);
	EXPECT_TRUE (old.yieldIfOvertaken (12.2));
	EXPECT_FALSE (old.isMaster());
}
