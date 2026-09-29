#include <gtest/gtest.h>

#include "FollowLeader.h"

namespace
{
	FollowInput playingAt (double leaderBpm, double gridBpm)
	{
		FollowInput in;
		in.leaderBpm = leaderBpm;
		in.leaderPlaying = true;
		in.followerGridBpm = gridBpm;
		in.followerPlaying = true;
		return in;
	}
}

TEST (FollowLeader, TempoMatchesTheLeader)
{
	const auto r = followLeader (playingAt (128.0, 125.0));
	EXPECT_DOUBLE_EQ (r.multiple, 1.0);
	ASSERT_TRUE (r.tempo.has_value());
	EXPECT_NEAR (*r.tempo, 1.024, 1e-9);
}

TEST (FollowLeader, TheMultipleWithTheSmallestChangeIsTaken)
{
	auto in = playingAt (140.0, 70.0);
	in.followerSpeed = 1.5;
	const auto r = followLeader (in);
	EXPECT_DOUBLE_EQ (r.multiple, 0.5);
	ASSERT_TRUE (r.tempo.has_value());
	EXPECT_NEAR (*r.tempo, 1.0, 1e-9);
}

TEST (FollowLeader, TheMultipleIsChosenOnceAndKept)
{
	auto in = playingAt (128.0, 128.0);
	in.multiple = 2.0;
	const auto r = followLeader (in);
	EXPECT_DOUBLE_EQ (r.multiple, 2.0);
	ASSERT_TRUE (r.tempo.has_value());
	EXPECT_NEAR (*r.tempo, 2.0, 1e-9);
}

TEST (FollowLeader, NoTempoWhenItIsAlreadyRight)
{
	auto in = playingAt (128.0, 128.0);
	in.followerSpeed = 1.0;
	EXPECT_FALSE (followLeader (in).tempo.has_value());
}

TEST (FollowLeader, FarOffJumpsIntoPhase)
{
	auto in = playingAt (120.0, 120.0);
	in.leaderBeatPhase = 10.0;
	in.followerBeatPhase = 20.25;   // a quarter beat ahead: 125 ms at 120 BPM
	const auto r = followLeader (in);
	ASSERT_TRUE (r.jumpBeats.has_value());
	EXPECT_NEAR (*r.jumpBeats, -0.25, 1e-9);
	EXPECT_DOUBLE_EQ (r.nudge, 1.0);
}

TEST (FollowLeader, CloseIsNudged)
{
	auto in = playingAt (120.0, 120.0);
	in.leaderBeatPhase = 5.02;      // follower 10 ms behind: 1.5 x 0.01
	in.followerBeatPhase = 7.0;
	const auto r = followLeader (in);
	EXPECT_FALSE (r.jumpBeats.has_value());
	EXPECT_NEAR (r.nudge, 1.015, 1e-9);
}

TEST (FollowLeader, TheNudgeIsCappedAtTwoPercent)
{
	auto in = playingAt (120.0, 120.0);
	in.leaderBeatPhase = 5.08;      // 40 ms behind: 1.5 x 0.04 = 6 %, capped
	in.followerBeatPhase = 7.0;
	EXPECT_NEAR (followLeader (in).nudge, 1.02, 1e-9);
}

TEST (FollowLeader, NoPhaseWhileNotPlayingOrScratching)
{
	auto stopped = playingAt (128.0, 125.0);
	stopped.followerPlaying = false;
	stopped.leaderBeatPhase = 0.3;
	auto scratching = playingAt (128.0, 125.0);
	scratching.anyScratching = true;
	scratching.leaderBeatPhase = 0.3;

	for (const auto& in : { stopped, scratching })
	{
		const auto r = followLeader (in);
		EXPECT_DOUBLE_EQ (r.nudge, 1.0);
		EXPECT_FALSE (r.jumpBeats.has_value());
		EXPECT_TRUE (r.tempo.has_value()) << "the tempo still follows";
	}
}
