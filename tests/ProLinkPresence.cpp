// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "ProLinkPresence.h"

// A CDJ-3000 announces, claims its number and sends keep-alives from the
// moment it is plugged in, whatever its SYNC and MASTER keys say. Only the
// beats of a master deck wait for MASTER.
TEST (ProLinkPresence, OnTheLinkWithNeitherSyncNorMaster)
{
	const auto presence = prolink::presence (true, false);
	EXPECT_TRUE (presence.receiver) << "announce and claim as soon as the ports are known";
	EXPECT_FALSE (presence.sender) << "no master deck: nothing to publish";
}

TEST (ProLinkPresence, AMasterDeckAlsoSends)
{
	const auto presence = prolink::presence (true, true);
	EXPECT_TRUE (presence.receiver);
	EXPECT_TRUE (presence.sender);
}

TEST (ProLinkPresence, OffTheLinkWithoutPorts)
{
	const auto presence = prolink::presence (false, true);
	EXPECT_FALSE (presence.receiver) << "no a3-osc.json: no ports to use";
	EXPECT_FALSE (presence.sender);
}
