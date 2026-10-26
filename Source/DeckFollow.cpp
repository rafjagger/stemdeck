// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "DeckFollow.h"

#include "DeckPanel.h"
#include "StemDeckPlayer.h"

#include <cmath>

namespace deckfollow
{
	void setBent (DeckPanel& panel, StemDeckPlayer& player, bool& bent, bool value)
	{
		bent = value;
		panel.setSyncBpmOnly (value);
		if (value)
			player.setSyncNudge (1.0);
	}

	double apply (DeckPanel& panel, StemDeckPlayer& follower, bool& bent, FollowInput in, double followerPosition)
	{
		const auto grid = follower.getBeatGrid();

		if (std::abs (follower.getPitchBend() - 1.0) > 1e-4 && ! bent)
			setBent (panel, follower, bent, true);
		if (bent)
			in.leaderPlaying = false;   // the tempo only: the beat is the hand's

		in.followerGridBpm = grid.bpm;
		in.followerBeatPhase = grid.beatsAt (followerPosition);
		in.followerSpeed = follower.getSpeed();
		in.followerEffectiveRate = follower.getEffectiveRate();
		in.followerPlaying = follower.isPlaying();

		const auto out = followLeader (in);

		if (out.tempo)
			panel.setTempoFromSync (*out.tempo);

		if (out.jumpBeats)
			follower.setPosition (follower.getPosition() + *out.jumpBeats * grid.beatLength());

		follower.setSyncNudge (out.nudge);
		return out.multiple;
	}
}
