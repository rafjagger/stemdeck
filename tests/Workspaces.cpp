#include <gtest/gtest.h>

#include "Workspaces.h"

TEST (Workspaces, TheLabelIsTheNameAfterTheNumber)
{
	EXPECT_EQ (workspaceLabel ("3:REAPER"), "REAPER");
	EXPECT_EQ (workspaceLabel ("1:MOTION"), "MOTION");
	EXPECT_EQ (workspaceLabel ("7"), "7");
	EXPECT_EQ (workspaceLabel ("scratch"), "scratch");
}

TEST (Workspaces, TheNumberIsWhatI3GoesTo)
{
	EXPECT_EQ (workspaceNumber ("3:REAPER"), 3);
	EXPECT_EQ (workspaceNumber ("12"), 12);
	EXPECT_EQ (workspaceNumber ("scratch"), 0);
}

// The switch sits where A3 Motion's does, so the key under the finger stays
// put when the workspace changes. At the rig's 768 px: StemDeck's first sizes.
TEST (Workspaces, TheSwitchIsWhereMotionHasIt)
{
	const auto s = switcherGeometry (768);
	EXPECT_EQ (s.margin, 6);
	EXPECT_EQ (s.arrowWidth, 30);
	EXPECT_EQ (s.gap, 2);
	EXPECT_EQ (s.appKeyWidth, 80);
	EXPECT_EQ (s.listTop, 40);
	EXPECT_EQ (s.listWidth, 170);
	EXPECT_EQ (s.listKeyHeight, 40);
	EXPECT_EQ (s.listGap, 4);
}

TEST (Workspaces, TheSwitchScalesWithTheWindow)
{
	const auto s = switcherGeometry (1536);
	EXPECT_EQ (s.arrowWidth, 60);
	EXPECT_EQ (s.appKeyWidth, 160);
	EXPECT_EQ (s.listWidth, 340);
}
