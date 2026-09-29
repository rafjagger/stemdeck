#include <gtest/gtest.h>

#include "PioneerClock.h"

namespace
{
	prolink::BeatPacket beat (int device, double bpm)
	{
		prolink::BeatPacket b;
		b.device = device;
		b.trackBpm = bpm;
		b.effectiveBpm = bpm;
		return b;
	}

	prolink::StatusPacket status (int device, bool master, bool playing = true)
	{
		prolink::StatusPacket s;
		s.device = device;
		s.isMaster = master;
		s.isPlaying = playing;
		return s;
	}
}

TEST (PioneerClock, ThePhaseRunsFromZeroToOneBetweenBeats)
{
	PioneerClock clock;
	clock.onBeat (beat (1, 128.0), 10.0);
	EXPECT_NEAR (clock.beatPhaseAt (10.0), 0.0, 1e-9);
	EXPECT_NEAR (clock.beatPhaseAt (10.0 + 60.0 / 128.0 / 2.0), 0.5, 1e-9);
	EXPECT_DOUBLE_EQ (clock.bpm(), 128.0);
}

TEST (PioneerClock, TheMasterLeads)
{
	PioneerClock clock;
	clock.onStatus (status (3, true), 9.9);
	clock.onBeat (beat (2, 120.0), 10.0);
	clock.onBeat (beat (3, 126.0), 10.1);
	EXPECT_EQ (clock.leader (10.1), 3);
	EXPECT_DOUBLE_EQ (clock.bpm(), 126.0);
}

TEST (PioneerClock, BeatsFromOthersAreIgnored)
{
	PioneerClock clock;
	clock.onStatus (status (3, true), 9.9);
	clock.onBeat (beat (3, 126.0), 10.0);
	clock.onBeat (beat (2, 120.0), 10.2);
	EXPECT_DOUBLE_EQ (clock.bpm(), 126.0);
	EXPECT_NEAR (clock.beatPhaseAt (10.2), 0.2 * 126.0 / 60.0, 1e-9) << "the phase still counts from 3's beat";
}

TEST (PioneerClock, WithoutStatusTheFirstBeaterLeads)
{
	PioneerClock clock;
	clock.onBeat (beat (2, 124.0), 10.0);
	clock.onBeat (beat (4, 130.0), 10.1);
	EXPECT_FALSE (clock.hasMasterInfo (10.1));
	EXPECT_EQ (clock.leader (10.1), 2);
	EXPECT_EQ (clock.chosenPlayer(), 2) << "remembered as the choice";
	EXPECT_DOUBLE_EQ (clock.bpm(), 124.0);
}

TEST (PioneerClock, AChosenPlayerLeadsWithoutMasterInfo)
{
	PioneerClock clock;
	clock.choosePlayer (4);
	clock.onBeat (beat (2, 124.0), 10.0);
	clock.onBeat (beat (4, 130.0), 10.1);
	EXPECT_EQ (clock.leader (10.1), 4);
	EXPECT_DOUBLE_EQ (clock.bpm(), 130.0);
}

TEST (PioneerClock, StatusOlderThanTwoSecondsIsNoMasterInfo)
{
	PioneerClock clock;
	clock.choosePlayer (2);
	clock.onStatus (status (3, true), 10.0);
	EXPECT_TRUE (clock.hasMasterInfo (11.9));
	EXPECT_EQ (clock.leader (11.9), 3);
	EXPECT_FALSE (clock.hasMasterInfo (12.1));
	EXPECT_EQ (clock.leader (12.1), 2);
}

TEST (PioneerClock, SilenceHoldsTheTempoAndStopsThePhase)
{
	PioneerClock clock;
	clock.onBeat (beat (1, 128.0), 10.0);
	EXPECT_TRUE (clock.isLive (11.9));
	EXPECT_FALSE (clock.isLive (12.1));
	EXPECT_DOUBLE_EQ (clock.bpm(), 128.0) << "the tempo is held";
}

TEST (PioneerClock, AMasterHandoverFollowsTheNewMaster)
{
	PioneerClock clock;
	clock.onStatus (status (2, true), 9.9);
	clock.onBeat (beat (2, 120.0), 10.0);
	clock.onStatus (status (2, false), 10.1);
	clock.onStatus (status (4, true), 10.1);
	clock.onBeat (beat (2, 120.0), 10.3);
	clock.onBeat (beat (4, 128.0), 10.4);
	EXPECT_EQ (clock.leader (10.4), 4);
	EXPECT_DOUBLE_EQ (clock.bpm(), 128.0);
	EXPECT_NEAR (clock.beatPhaseAt (10.4), 0.0, 1e-9) << "the phase starts from 4's beat, not 2's";
}

// Review 2026-09-29: status packets arrive, but nobody is master and playing
// (the master paused, or the master is the mixer): the chosen or the first
// player still leads -- following nothing would leave the deck stranded.
TEST (PioneerClock, WithStatusButNoMasterTheFirstBeaterLeads)
{
	PioneerClock clock;
	clock.onStatus (status (2, false, true), 9.9);
	clock.onBeat (beat (2, 124.0), 10.0);
	EXPECT_TRUE (clock.hasMasterInfo (10.0));
	EXPECT_FALSE (clock.hasMaster (10.0));
	EXPECT_EQ (clock.leader (10.0), 2);
	EXPECT_DOUBLE_EQ (clock.bpm(), 124.0);
}

TEST (PioneerClock, APausedMasterHandsTheLeadToTheChosenPlayer)
{
	PioneerClock clock;
	clock.choosePlayer (3);
	clock.onStatus (status (2, true), 9.9);
	EXPECT_EQ (clock.leader (9.9), 2);
	clock.onStatus (status (2, true, false), 10.0); // master, but paused
	EXPECT_EQ (clock.leader (10.0), 3);
}
