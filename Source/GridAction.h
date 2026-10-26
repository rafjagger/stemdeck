// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Grid Adjust, as on a CDJ-3000: GRID turns it on; the jog wheel then moves
// the grid, and <1/2 1/2> <1 1> (the one a beat) SNAP (the downbeat onto the cue) SHIFT (take the
// phase aligned by ear against the sync leader) RESET (as analysed) edit it.
// Its own header: a deck's grid is edited without any of the deck panel's UI.
enum class GridAction { shift, halfBack, halfForward, oneBack, oneForward, doubleTempo, halveTempo, snapToCue, downbeatAtPlayhead, shiftToLeader, reset };
