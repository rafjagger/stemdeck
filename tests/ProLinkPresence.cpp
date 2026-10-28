// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "ProLinkPresence.h"

// A CDJ-3000 announces, claims its number, sends keep-alives and a status
// from the moment it is plugged in, whatever its SYNC and MASTER keys say.
// Only the beats wait for MASTER -- the sender's business, not the presence's.
TEST (ProLinkPresence, OnTheLinkAndSendingStatusAsSoonAsThePortsAreKnown)
{
	const auto presence = prolink::presence (true);
	EXPECT_TRUE (presence.receiver) << "announce and claim as soon as the ports are known";
	EXPECT_TRUE (presence.sender) << "a status every 200 ms, master or not";
}

TEST (ProLinkPresence, OffTheLinkWithoutPorts)
{
	const auto presence = prolink::presence (false);
	EXPECT_FALSE (presence.receiver) << "no a3-osc.json: no ports to use";
	EXPECT_FALSE (presence.sender);
}
