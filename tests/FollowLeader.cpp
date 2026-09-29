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

TEST (FollowLeader, WithBarsTheDownbeatsLineUp)
{
	auto in = playingAt (120.0, 120.0);
	in.alignBars = true;
	in.leaderBeatPhase = 8.0;       // on a 1
	in.followerBeatPhase = 13.0;    // on a 2: one beat ahead in the bar
	const auto r = followLeader (in);
	ASSERT_TRUE (r.jumpBeats.has_value());
	EXPECT_NEAR (*r.jumpBeats, -1.0, 1e-9);
}

TEST (FollowLeader, WithBarsTheShortWayRound)
{
	auto in = playingAt (120.0, 120.0);
	in.alignBars = true;
	in.leaderBeatPhase = 8.0;       // on a 1
	in.followerBeatPhase = 15.1;    // on a 4, a little past: 0.9 beats to the next 1
	const auto r = followLeader (in);
	ASSERT_TRUE (r.jumpBeats.has_value());
	EXPECT_NEAR (*r.jumpBeats, 0.9, 1e-9);
}

TEST (FollowLeader, WithBarsInTheRightBeatItIsTheUsualNudge)
{
	auto in = playingAt (120.0, 120.0);
	in.alignBars = true;
	in.leaderBeatPhase = 5.02;
	in.followerBeatPhase = 9.0;     // both in beat 2 of the bar, 10 ms apart
	const auto r = followLeader (in);
	EXPECT_FALSE (r.jumpBeats.has_value());
	EXPECT_NEAR (r.nudge, 1.015, 1e-9);
}

TEST (FollowLeader, BarsOnlyAtTheSameTempo)
{
	auto in = playingAt (120.0, 120.0);
	in.alignBars = true;
	in.multiple = 2.0;
	in.leaderBeatPhase = 4.0;
	in.followerBeatPhase = 13.0;    // beats in phase, bars can't be at double tempo
	EXPECT_FALSE (followLeader (in).jumpBeats.has_value());
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

// Review 2026-09-29: a deck's position moves once per audio block, the Pioneer
// phase is computed for "now". Compared raw, the gap jitters by up to one block
// and the nudge wobbles the pitch. The position is carried forward to "now".
TEST (FollowLeader, ThePositionIsCarriedForwardToNow)
{
	EXPECT_NEAR (positionAt (10.0, 100.000, 100.010, 1.0, true), 10.010, 1e-9);
	EXPECT_NEAR (positionAt (10.0, 100.000, 100.010, 1.02, true), 10.0102, 1e-9);
}

TEST (FollowLeader, AStoppedDeckIsNotCarried)
{
	EXPECT_DOUBLE_EQ (positionAt (10.0, 100.0, 100.5, 1.0, false), 10.0);
}

TEST (FollowLeader, ACarryIsNeverBackwardsNorLongerThanABlockCouldBe)
{
	EXPECT_DOUBLE_EQ (positionAt (10.0, 100.0, 99.9, 1.0, true), 10.0) << "stamp from the future";
	EXPECT_NEAR (positionAt (10.0, 100.0, 100.04, 1.0, true), 10.04, 1e-9) << "a block's worth is carried";
}

// Review 2026-09-29 (I1): right after PLAY the stamp is from before the pause.
// Carrying it would put the deck up to 100 ms ahead, and the beat it is
// standing on -- a cue on the downbeat -- would never be sent. A stamp older
// than any audio block says nothing about now: not carried.
TEST (FollowLeader, AStaleStampIsNotCarried)
{
	EXPECT_DOUBLE_EQ (positionAt (10.0, 100.0, 105.0, 1.0, true), 10.0);
	EXPECT_DOUBLE_EQ (positionAt (10.0, 100.0, 100.2, 1.0, true), 10.0);
}

TEST (FollowLeader, TheNextBeatIsTheNextGridLine)
{
	const auto next = nextBeat (0.5, 120.0, 1.2);
	ASSERT_TRUE (next.has_value());
	EXPECT_NEAR (next->trackSeconds, 1.5, 1e-9);
	EXPECT_EQ (next->beatInBar, 3);
}

TEST (FollowLeader, OnABeatTheNextIsTheOneAfter)
{
	const auto next = nextBeat (0.5, 120.0, 1.5);
	ASSERT_TRUE (next.has_value());
	EXPECT_NEAR (next->trackSeconds, 2.0, 1e-9);
	EXPECT_EQ (next->beatInBar, 4);
}

TEST (FollowLeader, BeforeTheFirstBeatItIsTheFirst)
{
	const auto next = nextBeat (0.5, 120.0, 0.1);
	ASSERT_TRUE (next.has_value());
	EXPECT_NEAR (next->trackSeconds, 0.5, 1e-9);
	EXPECT_EQ (next->beatInBar, 1);
}

TEST (FollowLeader, TheBarWraps)
{
	const auto next = nextBeat (0.5, 120.0, 2.1); // the fifth beat, at 2.5 s
	ASSERT_TRUE (next.has_value());
	EXPECT_NEAR (next->trackSeconds, 2.5, 1e-9);
	EXPECT_EQ (next->beatInBar, 1);
}

TEST (FollowLeader, NoTempoNoBeat)
{
	EXPECT_FALSE (nextBeat (0.5, 0.0, 1.0).has_value());
}

TEST (FollowLeader, AJumpBackIsNotABurst)
{
	const auto before = nextBeat (0.5, 120.0, 3.9);
	const auto after = nextBeat (0.5, 120.0, 1.2);
	ASSERT_TRUE (before.has_value());
	ASSERT_TRUE (after.has_value());
	EXPECT_NEAR (before->trackSeconds, 4.0, 1e-9);
	EXPECT_NEAR (after->trackSeconds, 1.5, 1e-9) << "just the next one after the jump";
}
