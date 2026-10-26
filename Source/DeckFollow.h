// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "FollowLeader.h"

class DeckPanel;
class StemDeckPlayer;

// One synced deck put on its leader -- the other deck, or the link's master:
// the rules in followLeader(), the result on the deck's fader and player.
// Shared by the two-deck screen and the one-deck screen (S4.7).
namespace deckfollow
{
	// SYNC on but only the tempo followed: the pitch bent by hand (CDJ-3000),
	// until SHIFT GRID. Bent, the phase nudge is let go.
	void setBent (DeckPanel&, StemDeckPlayer&, bool& bent, bool value);

	// `in` carries the leader's half; the follower's is read here, at
	// `followerPosition`. A hand on the pitch bend makes the deck bent. Returns
	// the multiple, to pass back in on the next call.
	double apply (DeckPanel&, StemDeckPlayer&, bool& bent, FollowInput in, double followerPosition);
}
