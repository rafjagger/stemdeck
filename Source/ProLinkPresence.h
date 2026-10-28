// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace prolink
{
	// What of StemDeck is on the Pro DJ Link. As a CDJ-3000: on the link as
	// soon as it is plugged in -- announce, claim the number, keep-alives --
	// whatever SYNC and MASTER say. Status and beats go out for a master deck.
	struct Presence
	{
		bool receiver = false;   // ProLinkReceiver: announce, claim, keep-alive, listen
		bool sender = false;     // ProLinkSender: the master deck's status and beats
	};

	inline Presence presence (bool portsKnown, bool hasMasterDeck)
	{
		return { portsKnown, portsKnown && hasMasterDeck };
	}
}
