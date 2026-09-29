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

	// Also line the bars up: the follower's downbeat (its grid's first beat,
	// 1 of 4) on the leader's -- as a CDJ's BEAT SYNC does. Only at the same
	// tempo (multiple 1); a whole-beat difference jumps.
	bool alignBars = false;
};

struct FollowResult
{
	double multiple = 1.0;
	std::optional<double> tempo;     // the speed to set, when it changes
	std::optional<double> jumpBeats; // jump the follower by this many of its beats
	double nudge = 1.0;              // 1: no correction
};

FollowResult followLeader (const FollowInput& in);

// A deck's position carried from when the audio thread last moved it
// (`stampSeconds`) to `nowSeconds` at `rate` track-seconds per second -- so it
// can be set against a leader phase computed for now. Not while stopped or
// scratching; never backwards; not at all from a stamp older than 100 ms --
// longer than any audio block, so it is from before a pause or a stall.
double positionAt (double position, double stampSeconds, double nowSeconds, double rate, bool moving);

// Part 2, StemDeck as the tempo master: the next beat of a deck's grid after
// `position` (track seconds) -- when it falls, and where in the bar. The bar
// counts from the grid's first beat: that beat is 1. Nothing without a tempo.
// One beat only: after a jump back it is simply the next one, never a burst
// of the beats skipped over.
struct NextBeat
{
	double trackSeconds = 0.0;
	int beatInBar = 1;
};

std::optional<NextBeat> nextBeat (double gridFirstBeat, double gridBpm, double position);
