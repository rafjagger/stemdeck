// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "TransportDeck.h"

// LOOP IN, LOOP OUT and LOOP (RELOOP/EXIT), like a Pioneer CDJ-3000's
// (manual, "Looping"). Pure: no JUCE.
//
//   IN:    marks the loop's start at the playhead
//   OUT:   after a mark, the loop from it to the playhead, playing at once
//          from its start; without a mark, or before it, nothing
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
	// The SCS.3d's VINYL key: OUT when a mark lies before the playhead, IN otherwise.
	void inOrOut (TransportDeck&);

	bool isInMarked() const { return inMarked; }
	// LOOP has something to do: a loop running -- whoever set it, the
	// overview drag or a session too -- or one to go back into.
	bool canReloop (const TransportDeck& deck) const { return deck.hasLoop() || hasStored; }
	// A new track: no mark, nothing to go back into.
	void reset();

private:
	bool canCloseAt (double position) const;

	bool inMarked = false, hasStored = false;
	double inPoint = 0.0, storedStart = 0.0, storedEnd = 0.0;
};
