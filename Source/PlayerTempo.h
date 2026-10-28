// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Remote.h"
#include "StemDeckPlayer.h"

// What a deck tells Core about its clock (remote::TempoReport), read from the
// player as the deck panel shows it: the grid's tempo times the rate (the
// pitch), 0 with no track or no grid. `master` is what the deck shows as
// MASTER, which only the caller knows.
inline remote::Tempo tempoOfPlayer (const StemDeckPlayer& player, bool master)
{
	const auto grid = player.getBeatGrid();
	return { remote::tempoOf (player.isLoaded(), grid.isValid() ? grid.bpm : 0.0, player.getEffectiveRate()),
			 player.isPlaying(), master };
}
