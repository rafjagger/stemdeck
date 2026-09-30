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
