#pragma once

#include "FollowLeader.h"

#include <optional>

// When the master deck's beats go out (Part 2): one decision per wake-up of
// the sender thread. Pure -- the thread sleeps and sends what it is told --
// so the timing can be simulated in a test.
//
//   - A beat due within two wake-ups is waited for exactly and sent on time.
//     (Within one wake-up was not enough: a sleep always overruns a little,
//     and a beat 5.0x ms away slipped past unsent.)
//   - A beat crossed since the last wake-up and not yet sent is sent at once,
//     unless the deck jumped (cue, loop, drag) -- then it is not a beat that
//     was played.
//   - Starting on a beat (a cue on the downbeat) sends that beat.
//   - Never two beats within half a beat, whichever deck they come from: a
//     master handed over mid-beat does not double the beat.
struct BeatStep
{
	double sleepBefore = 0.0;       // sleep this long, then send (if any)
	std::optional<NextBeat> send;
};

class BeatScheduler
{
public:
	static constexpr double wakeEvery = 0.005;

	// `position` is the master deck's position at `now`, already carried
	// (positionAt); `rate` its effective rate.
	BeatStep step (double now, double position, double rate, double gridFirstBeat, double gridBpm, bool moving);

private:
	bool mayDouble (double at, double beatSeconds) const;

	double lastNow = -1.0, lastPosition = -1.0, lastSentAt = -1.0;
};
