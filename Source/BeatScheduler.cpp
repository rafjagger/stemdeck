#include "BeatScheduler.h"

#include <cmath>

namespace
{
	constexpr double onBeatTolerance = 0.01;  // track seconds: "standing on a beat"
	constexpr double jumpTolerance = 0.05;    // more than the carry can explain: a jump
}

bool BeatScheduler::mayDouble (double at, double beatSeconds) const
{
	return lastSentAt >= 0.0 && at - lastSentAt < 0.5 * beatSeconds;
}

BeatStep BeatScheduler::step (double now, double position, double rate, double first, double bpm, bool moving)
{
	if (! moving || bpm <= 0.0 || rate <= 0.0)
	{
		lastPosition = -1.0;
		return { wakeEvery, std::nullopt };
	}

	const auto beatLength = 60.0 / bpm;           // track seconds
	const auto beatSeconds = beatLength / rate;   // real seconds

	const auto justStarted = lastPosition < 0.0;
	const auto jumped = ! justStarted
						&& (position < lastPosition || position - lastPosition > (now - lastNow) * rate + jumpTolerance);
	const auto previous = lastPosition;
	lastNow = now;
	lastPosition = position;

	const auto sendNow = [&] (NextBeat beat) -> BeatStep
	{
		lastSentAt = now;
		return { 0.0, beat };
	};

	// Starting on a beat -- a cue on the downbeat: that beat is played.
	if (justStarted && position >= first)
	{
		const auto index = std::floor ((position - first) / beatLength + 1e-9);
		const auto on = first + index * beatLength;
		if (position - on < onBeatTolerance && ! mayDouble (now, beatSeconds))
			return sendNow ({ on, 1 + (int) ((long long) index % 4) });
	}

	// Crossed since the last wake-up and not sent: late, but not lost.
	if (! justStarted && ! jumped)
		if (const auto crossed = nextBeat (first, bpm, previous); crossed && crossed->trackSeconds <= position
																	&& ! mayDouble (now, beatSeconds))
			return sendNow (*crossed);

	// Due within two wake-ups: wait for it exactly.
	const auto next = nextBeat (first, bpm, position);
	if (! next)
		return { wakeEvery, std::nullopt };

	const auto wait = (next->trackSeconds - position) / rate;
	if (wait > 2.0 * wakeEvery || mayDouble (now + wait, beatSeconds))
		return { wakeEvery, std::nullopt };

	lastSentAt = now + wait;
	lastNow = now + wait;
	lastPosition = next->trackSeconds;  // where the deck is once it has been sent
	return { wait, next };
}
