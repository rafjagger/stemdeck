// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "TransportDeck.h"

// LOOP IN, LOOP OUT and LOOP (RELOOP/EXIT), like a Pioneer CDJ-3000's
// (manual, "Looping"). Pure: no JUCE.
//
//   IN:    marks the loop's start at the playhead
//   OUT:   right after IN, the loop from the mark to the playhead, playing at
//          once from its start; while looping, leaves the loop and plays on;
//          pressed again, the loop from the same mark to the new playhead --
//          a toggle (the maintainer, 2026-10-28). Without a mark, or before
//          it, it closes nothing.
//   LOOP:  looping, leaves the loop and plays on (EXIT); otherwise goes back
//          into the last loop from its start (RELOOP)
//
// The deck puts both ends on its beat grid when it has one (the quantize,
// StemDeckPlayer::setLoop).
class DeckLoop
{
public:
	void in (TransportDeck&);
	void out (TransportDeck&);
	void toggle (TransportDeck&);
	// The SCS.3d's VINYL key: OUT when an IN pressed since the last loop lies
	// before the playhead, IN otherwise -- it never leaves a loop.
	void inOrOut (TransportDeck&);

	bool isInMarked() const { return inMarked; }
	// LOOP has something to do: a loop running -- whoever set it, the
	// overview drag or a session too -- or one to go back into.
	bool canReloop (const TransportDeck& deck) const { return deck.hasLoop() || hasStored; }
	// A new track: no mark, nothing to go back into.
	void reset();

private:
	bool canCloseAt (double position) const;
	void close (TransportDeck&);
	void exit (TransportDeck&);

	// freshIn: IN pressed since the last loop was closed -- OUT then closes
	// rather than leaves.
	bool inMarked = false, freshIn = false, hasStored = false;
	double inPoint = 0.0, storedStart = 0.0, storedEnd = 0.0;
};
