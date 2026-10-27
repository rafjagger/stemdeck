#include <gtest/gtest.h>

#include "Workspaces.h"

#include <vector>

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

// No i3-msg runs in these tests: the runner is the seam.
namespace
{
	struct FakeI3
	{
		std::vector<std::vector<std::string>> calls;
		std::string reply;
		I3Runner runner() { return [this] (const std::vector<std::string>& argv) { calls.push_back (argv); return reply; }; }
	};
}

TEST (Workspaces, GoingToAWorkspaceIsOneI3MsgCommand)
{
	FakeI3 i3;
	goToWorkspace (i3.runner(), 5);
	ASSERT_EQ (i3.calls.size(), 1u);
	EXPECT_EQ (i3.calls[0], (std::vector<std::string> { "i3-msg", "-q", "workspace number 5" }));
}

TEST (Workspaces, TheListIsWhatI3Reports)
{
	FakeI3 i3;
	i3.reply = R"([{"name":"1:MOTION","focused":false},{"name":"2:STEMDECK","focused":true},{"name":"scratch","focused":false}])";
	const auto list = listWorkspaces (i3.runner());
	EXPECT_EQ (i3.calls.at (0), (std::vector<std::string> { "i3-msg", "-t", "get_workspaces" }));
	ASSERT_EQ (list.size(), 2u);   // "scratch" has no number to go to
	EXPECT_EQ (list[0].number, 1);
	EXPECT_EQ (list[0].label, "MOTION");
	EXPECT_FALSE (list[0].current);
	EXPECT_EQ (list[1].number, 2);
	EXPECT_TRUE (list[1].current);
}

TEST (Workspaces, NoAnswerOrNoJsonIsAnEmptyList)
{
	FakeI3 i3;
	EXPECT_TRUE (listWorkspaces (i3.runner()).empty());
	i3.reply = "not json";
	EXPECT_TRUE (listWorkspaces (i3.runner()).empty());
}
