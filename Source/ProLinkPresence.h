// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace prolink
{
	// What of StemDeck is on the Pro DJ Link. As a CDJ-3000: on the link as
	// soon as it is plugged in -- announce, claim the number, keep-alives and a
	// status every 200 ms -- whatever SYNC and MASTER say. MASTER only adds the
	// beats, inside the sender (ProLinkSender::setMaster).
	struct Presence
	{
		bool receiver = false;   // ProLinkReceiver: announce, claim, keep-alive, listen
		bool sender = false;     // ProLinkSender: status and position; beats for a MASTER deck
	};

	inline Presence presence (bool portsKnown)
	{
		return { portsKnown, portsKnown };
	}
}
