#pragma once

#include <optional>

// How a synced deck follows its leader -- the other deck, or the Pioneer
// master. The maths of MainComponent::updateSync(), taken out unchanged so
// both leaders use one set of rules and the rules can be tested:
//   tempo: the leader's, times the half/same/double multiple that needs the
//          smallest change, chosen once per sync;
//   phase: only while both play and nobody scratches; more than 50 ms off
//          jumps into phase, closer is nudged by at most 2 %.
struct FollowInput
{
	double leaderBpm = 0.0;          // the leader's tempo as heard
	double leaderBeatPhase = 0.0;    // the leader's position in its beats (fractional)
	bool leaderPlaying = false;

	double followerGridBpm = 0.0;    // the follower's track tempo (its grid)
	double followerBeatPhase = 0.0;  // the follower's position in its beats
	double followerSpeed = 1.0;      // its tempo fader
	double followerEffectiveRate = 1.0;
	bool followerPlaying = false;

	bool anyScratching = false;
	double multiple = 0.0;           // 0: choose now
};

struct FollowResult
{
	double multiple = 1.0;
	std::optional<double> tempo;     // the speed to set, when it changes
	std::optional<double> jumpBeats; // jump the follower by this many of its beats
	double nudge = 1.0;              // 1: no correction
};

FollowResult followLeader (const FollowInput& in);
