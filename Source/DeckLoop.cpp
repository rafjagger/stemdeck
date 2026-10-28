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
}

void DeckLoop::out (TransportDeck& deck)
{
	if (! canCloseAt (deck.position()))
		return;
	deck.setLoop (inPoint, deck.position());
	inMarked = false;
}

void DeckLoop::toggle (TransportDeck& deck)
{
	if (deck.hasLoop())
	{
		storedStart = deck.loopStart();
		storedEnd = deck.loopEnd();
		hasStored = true;
		deck.clearLoop();
		return;
	}
	if (hasStored && deck.isLoaded())
		deck.setLoop (storedStart, storedEnd);
}

void DeckLoop::inOrOut (TransportDeck& deck)
{
	if (canCloseAt (deck.position()))
		out (deck);
	else
		in (deck);
}

void DeckLoop::reset()
{
	inMarked = false;
	hasStored = false;
}

bool DeckLoop::canCloseAt (double position) const
{
	return inMarked && position > inPoint + shortestLoop;
}
