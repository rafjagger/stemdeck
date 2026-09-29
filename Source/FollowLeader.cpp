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
	const auto errorSeconds = error * beatLength / in.followerEffectiveRate; // in real time

	if (std::abs (errorSeconds) > 0.05)
		out.jumpBeats = error;  // far off (e.g. just started): jump into phase
	else
		out.nudge = 1.0 + std::clamp (errorSeconds * 1.5, -0.02, 0.02); // close: lean into it

	return out;
}
