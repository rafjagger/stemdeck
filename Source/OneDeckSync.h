// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "MasterDeck.h"
#include "PioneerClock.h"
#include "ProLinkReceiver.h"

#include <vector>

class DeckPanel;
class StemDeckPlayer;

// SYNC and MASTER of a one-deck instance (-bformat, S4.7, B17): decks in
// separate instances, on one host or several, meet on the link. The MASTER
// deck publishes (its ProLinkSender, the owner's); a synced deck follows the
// link's master -- another instance's MASTER deck, or a player -- with the
// two-deck app's rules (followLeader, DeckFollow). With no master heard the
// deck runs on its own grid. The newest MASTER press wins (InstanceMaster).
// No sockets here: the owner is on the link from the start (ProLinkPresence),
// passes on what it hears and publishes while isMaster().
class OneDeckSync
{
public:
	OneDeckSync (StemDeckPlayer&, DeckPanel&);

	void setSynced (bool synced);   // the SYNC key, or a restored session
	bool isSynced() const { return synced; }
	void pressMaster (double seconds);   // refused (stays off) on a deck without a grid
	bool isMaster() const { return master.isMaster(); }
	bool syncOrMaster() const { return synced || isMaster(); }

	void joined (double seconds);   // the owner's receiver started
	void hear (const std::vector<ProLinkReceiver::Event>&);

	// Every tick: MASTER given up to a newer claim (true then), and the deck
	// put on the link's master while synced and not master itself.
	bool update (double seconds);

	// Audio in flight between the read head and the ears; a synced deck lines
	// up as heard.
	void setPathLatency (double seconds) { pathLatency = seconds; }

	const PioneerClock& clock() const { return linkClock; }

private:
	StemDeckPlayer& player;
	DeckPanel& panel;
	PioneerClock linkClock;
	InstanceMaster master;
	bool synced = false, bent = false;
	double multiple = 0.0;
	double pathLatency = 0.0;
};
