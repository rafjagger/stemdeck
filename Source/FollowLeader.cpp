#include "FollowLeader.h"

#include <algorithm>
#include <cmath>

FollowResult followLeader (const FollowInput& in)
{
	FollowResult out;

	// Tempo: match the leader's tempo, taking whichever of half, same or
	// double tempo needs the smallest change (e.g. 70 BPM against 140).
	out.multiple = in.multiple;
	if (out.multiple <= 0.0)
	{
		out.multiple = 1.0;

		for (auto multiple : { 0.5, 2.0 })
			if (std::abs (std::log (in.leaderBpm * multiple / in.followerGridBpm)) < std::abs (std::log (in.leaderBpm * out.multiple / in.followerGridBpm)))
				out.multiple = multiple;
	}

	const auto rate = in.leaderBpm * out.multiple / in.followerGridBpm;

	if (std::abs (rate - in.followerSpeed) > 1e-6)
		out.tempo = rate;

	// Phase: only while both play and nobody scratches.
	if (! in.followerPlaying || ! in.leaderPlaying || in.anyScratching)
		return out;

	const auto leaderBeats = in.leaderBeatPhase * out.multiple;
	auto error = (leaderBeats - std::floor (leaderBeats)) - (in.followerBeatPhase - std::floor (in.followerBeatPhase));
	error -= std::round (error); // -0.5 .. 0.5 beats, positive: follower is behind

	const auto beatLength = 60.0 / in.followerGridBpm;

	if (in.alignBars && out.multiple == 1.0)
	{
		// Where each is in its bar; -2 .. 2 beats the short way round.
		constexpr double bar = 4.0;
		auto barError = std::fmod (leaderBeats, bar) - std::fmod (in.followerBeatPhase, bar);
		barError -= bar * std::round (barError / bar);
		if (std::abs (barError - error) >= 0.5)
		{
			out.jumpBeats = barError;   // on the wrong beat of the bar: jump to the right one
			return out;
		}
	}

	const auto errorSeconds = error * beatLength / in.followerEffectiveRate; // in real time

	if (std::abs (errorSeconds) > 0.05)
		out.jumpBeats = error;  // far off (e.g. just started): jump into phase
	else
		out.nudge = 1.0 + std::clamp (errorSeconds * 1.5, -0.02, 0.02); // close: lean into it

	return out;
}

double positionAt (double position, double stampSeconds, double nowSeconds, double rate, bool moving)
{
	// Longer than any audio block: a stamp this old is from before a pause or
	// a stall and says nothing about now.
	constexpr double longestCarry = 0.1;
	const auto age = nowSeconds - stampSeconds;
	if (! moving || age <= 0.0 || age > longestCarry)
		return position;
	return position + age * rate;
}

namespace
{
	int beatInBarOf (long long index)
	{
		return 1 + (int) (((index % 4) + 4) % 4);
	}

	// The index of the track's earliest beat, at or after 0 s.
	double earliestBeat (double gridFirstBeat, double beatLength)
	{
		return -std::floor (gridFirstBeat / beatLength + 1e-9);
	}
}

std::optional<NextBeat> nextBeat (double gridFirstBeat, double gridBpm, double position)
{
	if (gridBpm <= 0.0)
		return std::nullopt;

	const auto beatLength = 60.0 / gridBpm;
	const auto n = std::max (earliestBeat (gridFirstBeat, beatLength), std::floor ((position - gridFirstBeat) / beatLength) + 1.0);
	return NextBeat { gridFirstBeat + n * beatLength, beatInBarOf ((long long) n) };
}

std::optional<PassedBeat> beatPassed (double gridFirstBeat, double gridBpm, double position)
{
	if (gridBpm <= 0.0)
		return std::nullopt;

	const auto beatLength = 60.0 / gridBpm;
	const auto n = std::floor ((position - gridFirstBeat) / beatLength + 1e-9);
	if (n < earliestBeat (gridFirstBeat, beatLength))
		return std::nullopt;
	return PassedBeat { (long long) n, beatInBarOf ((long long) n) };
}
