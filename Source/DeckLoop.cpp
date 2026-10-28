// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "DeckLoop.h"

namespace
{
	// An OUT closer to the mark than this is a double press, not a loop.
	constexpr double shortestLoop = 0.05;
}

void DeckLoop::in (TransportDeck& deck)
{
	if (! deck.isLoaded())
		return;
	inPoint = deck.position();
	inMarked = true;
	freshIn = true;
}

void DeckLoop::out (TransportDeck& deck)
{
	if (! freshIn && deck.hasLoop())
		exit (deck);
	else
		close (deck);
}

void DeckLoop::toggle (TransportDeck& deck)
{
	if (deck.hasLoop())
		exit (deck);
	else if (hasStored && deck.isLoaded())
		deck.setLoop (storedStart, storedEnd);
}

void DeckLoop::inOrOut (TransportDeck& deck)
{
	if (freshIn && canCloseAt (deck.position()))
		close (deck);
	else
		in (deck);
}

void DeckLoop::reset()
{
	inMarked = false;
	freshIn = false;
	hasStored = false;
}

// The loop from the mark to the playhead; the mark stays for the next OUT.
void DeckLoop::close (TransportDeck& deck)
{
	if (! canCloseAt (deck.position()))
		return;
	deck.setLoop (inPoint, deck.position());
	freshIn = false;
}

// Out of the loop, playing on; LOOP goes back into it.
void DeckLoop::exit (TransportDeck& deck)
{
	storedStart = deck.loopStart();
	storedEnd = deck.loopEnd();
	hasStored = true;
	deck.clearLoop();
}

bool DeckLoop::canCloseAt (double position) const
{
	return inMarked && position > inPoint + shortestLoop;
}
