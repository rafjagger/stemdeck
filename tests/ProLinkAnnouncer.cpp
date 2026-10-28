// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "ProLinkAnnouncer.h"

#include <cstring>

// How a CDJ-3000 joins the link and holds its player number, after
// startup.html of the DJ Link analysis: three hellos, three claims in each of
// three stages, 300 ms apart; then a keep-alive every two seconds. Pure: the
// test is the clock and the network.
namespace
{
	uint32_t ip (int a, int b, int c, int d)
	{
		const uint8_t bytes[4] { (uint8_t) a, (uint8_t) b, (uint8_t) c, (uint8_t) d };
		uint32_t networkOrder = 0;
		std::memcpy (&networkOrder, bytes, 4);
		return networkOrder;
	}

	const uint32_t ourIp = ip (192, 168, 8, 20), theirIp = ip (192, 168, 8, 33);

	prolink::Identity us() { return { 6, "StemDeck", { 2, 0xa3, 0, 0, 8, 20 }, ourIp }; }
	prolink::Identity them (int device) { return { device, "CDJ-3000", { 2, 0, 0, 0, 0, 33 }, theirIp }; }

	// The type byte of every packet sent from `start` on, one call each `step`.
	std::vector<uint8_t> typesSent (prolink::Announcer& announcer, double start, double until, double step = 0.05)
	{
		std::vector<uint8_t> types;
		for (double t = start; t < until; t += step)
			for (const auto& send : announcer.due (t))
			{
				EXPECT_EQ (send.to, 0u) << "the startup is broadcast";
				types.push_back (send.bytes[0x0a]);
			}
		return types;
	}

	std::vector<prolink::Announcer::Send> hear (prolink::Announcer& announcer, const std::vector<uint8_t>& packet,
												uint32_t from, double now)
	{
		return announcer.hear (packet.data(), packet.size(), from, now);
	}
}

TEST (ProLinkAnnouncer, StartsUpAsACdjDoes)
{
	prolink::Announcer announcer (us());
	const auto types = typesSent (announcer, 0.0, 3.55);
	const std::vector<uint8_t> expected { 0x0a, 0x0a, 0x0a, 0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x04, 0x04, 0x04 };
	EXPECT_EQ (types, expected) << "hello x3, then the three claim stages x3, 300 ms apart";
	EXPECT_FALSE (announcer.isOnline()) << "still claiming";

	const auto settled = typesSent (announcer, 3.55, 3.65);
	EXPECT_EQ (settled, std::vector<uint8_t> { 0x06 }) << "300 ms after the last claim: the first keep-alive";
	EXPECT_TRUE (announcer.isOnline());
}

TEST (ProLinkAnnouncer, TheClaimCountsOneToThree)
{
	prolink::Announcer announcer (us());
	std::vector<int> counters;
	for (double t = 0.0; t < 3.55; t += 0.05)
		for (const auto& send : announcer.due (t))
			if (send.bytes[0x0a] == 0x00) counters.push_back (send.bytes[0x24]);
			else if (send.bytes[0x0a] == 0x02) counters.push_back (send.bytes[0x2f]);
			else if (send.bytes[0x0a] == 0x04) counters.push_back (send.bytes[0x25]);
	EXPECT_EQ (counters, (std::vector<int> { 1, 2, 3, 1, 2, 3, 1, 2, 3 }));
}

// startup.html: a CDJ keep-alive comes every 2.0 s, "held tightly".
TEST (ProLinkAnnouncer, KeepAlivesEveryTwoSeconds)
{
	prolink::Announcer announcer (us());
	typesSent (announcer, 0.0, 3.65);
	std::vector<double> at;
	for (double t = 3.65; t < 10.0; t += 0.01)
		for (const auto& send : announcer.due (t))
			if (send.bytes[0x0a] == 0x06)
				at.push_back (t);
	ASSERT_GE (at.size(), 3u);
	EXPECT_NEAR (at[1] - at[0], 2.0, 0.011);
	EXPECT_NEAR (at[2] - at[1], 2.0, 0.011);
}

// startup.html, "Channel conflicts": a player told its number is taken gives
// up on it. StemDeck's number is fixed per deck, so it stays off the link and
// says why.
TEST (ProLinkAnnouncer, ADefendedNumberIsGivenUp)
{
	prolink::Announcer announcer (us());
	typesSent (announcer, 0.0, 1.0);
	hear (announcer, prolink::conflict (them (6)), theirIp, 1.0);
	EXPECT_TRUE (typesSent (announcer, 1.0, 10.0).empty()) << "no more claims, no keep-alive";
	EXPECT_FALSE (announcer.isOnline());
	EXPECT_NE (announcer.refusal().find ("6"), std::string::npos) << announcer.refusal();
	EXPECT_NE (announcer.refusal().find ("192.168.8.33"), std::string::npos) << announcer.refusal();
}

// A device already holding the number that only sends keep-alives (beat-link,
// an older StemDeck) defends nothing: hearing it is enough.
TEST (ProLinkAnnouncer, ANumberHeardInUseIsGivenUp)
{
	prolink::Announcer announcer (us());
	typesSent (announcer, 0.0, 0.5);
	hear (announcer, prolink::keepAlive (them (6), 1, true), theirIp, 0.5);
	EXPECT_TRUE (typesSent (announcer, 0.5, 6.0).empty());
	EXPECT_FALSE (announcer.refusal().empty());
}

// Our own broadcasts come back to us: they are not a conflict.
TEST (ProLinkAnnouncer, OurOwnEchoIsNoConflict)
{
	prolink::Announcer announcer (us());
	typesSent (announcer, 0.0, 2.0);
	hear (announcer, prolink::claimStage2 (us(), 1), ourIp, 2.0);
	hear (announcer, prolink::keepAlive (us(), 1, true), ourIp, 2.0);
	typesSent (announcer, 2.0, 4.0);
	EXPECT_TRUE (announcer.isOnline());
	EXPECT_TRUE (announcer.refusal().empty());
}

// startup.html: an "assignment finished" packet cuts the final-stage series
// to the one already sent; the player goes straight to keep-alives.
TEST (ProLinkAnnouncer, AssignmentFinishedEndsTheFinalStage)
{
	prolink::Announcer announcer (us());
	const auto before = typesSent (announcer, 0.0, 2.75);
	ASSERT_FALSE (before.empty());
	ASSERT_EQ (before.back(), 0x04) << "the first final-stage claim is out";
	hear (announcer, prolink::assignmentFinished (them (2)), theirIp, 2.75);
	EXPECT_EQ (typesSent (announcer, 2.75, 3.1), std::vector<uint8_t> { 0x06 });
	EXPECT_TRUE (announcer.isOnline());
}

// Settled, StemDeck defends its number as a CDJ does: a conflict packet
// straight to whoever claims it.
TEST (ProLinkAnnouncer, ASettledPlayerDefendsItsNumber)
{
	prolink::Announcer announcer (us());
	typesSent (announcer, 0.0, 4.0);
	ASSERT_TRUE (announcer.isOnline());

	const auto replies = hear (announcer, prolink::claimStage2 (them (6), 1), theirIp, 4.0);
	ASSERT_EQ (replies.size(), 1u);
	EXPECT_EQ (replies[0].to, theirIp) << "unicast to the claimant";
	EXPECT_EQ (replies[0].bytes[0x0a], 0x08);
	EXPECT_EQ (replies[0].bytes[0x24], 6);
	EXPECT_TRUE (announcer.isOnline()) << "the number stays ours";

	EXPECT_EQ (hear (announcer, prolink::claimStage3 (them (6), 1), theirIp, 4.1).size(), 1u);
}

// startup.html: a settled player answers another number's final-stage claim
// with "assignment finished" -- why a player booting onto an occupied link
// sends only one.
TEST (ProLinkAnnouncer, ASettledPlayerFinishesAnothersClaim)
{
	prolink::Announcer announcer (us());
	typesSent (announcer, 0.0, 4.0);

	const auto replies = hear (announcer, prolink::claimStage3 (them (2), 1), theirIp, 4.0);
	ASSERT_EQ (replies.size(), 1u);
	EXPECT_EQ (replies[0].to, theirIp);
	EXPECT_EQ (replies[0].bytes[0x0a], 0x05);
	EXPECT_EQ (replies[0].bytes[0x24], 6) << "our own number";

	EXPECT_TRUE (hear (announcer, prolink::claimStage2 (them (2), 1), theirIp, 4.0).empty()) << "only the final stage";
}

// startup.html, keep-alive: p counts the devices on the link, itself
// included, and drops about ten seconds after one goes; byte 25 latches
// whether anyone was there before us.
TEST (ProLinkAnnouncer, TheKeepAliveCountsPeers)
{
	prolink::Announcer announcer (us());
	hear (announcer, prolink::keepAlive (them (2), 1, true), theirIp, 0.1);
	std::vector<uint8_t> lastKeepAlive;
	for (double t = 0.0; t < 4.0; t += 0.05)
		for (const auto& send : announcer.due (t))
			if (send.bytes[0x0a] == 0x06)
				lastKeepAlive = send.bytes;
	ASSERT_FALSE (lastKeepAlive.empty());
	EXPECT_EQ (lastKeepAlive[0x30], 2) << "the CDJ and us";
	EXPECT_EQ (lastKeepAlive[0x25], 0x01) << "someone was here first";

	for (double t = 4.0; t < 20.0; t += 0.05)
		for (const auto& send : announcer.due (t))
			if (send.bytes[0x0a] == 0x06)
				lastKeepAlive = send.bytes;
	EXPECT_EQ (lastKeepAlive[0x30], 1) << "the CDJ went quiet ten seconds ago";
	EXPECT_EQ (lastKeepAlive[0x25], 0x01) << "latched";
}

TEST (ProLinkAnnouncer, AloneOnTheLinkItWasFirst)
{
	prolink::Announcer announcer (us());
	std::vector<uint8_t> keepAlive;
	for (double t = 0.0; t < 4.0 && keepAlive.empty(); t += 0.05)
		for (const auto& send : announcer.due (t))
			if (send.bytes[0x0a] == 0x06)
				keepAlive = send.bytes;
	ASSERT_FALSE (keepAlive.empty());
	EXPECT_EQ (keepAlive[0x25], 0x02);
	EXPECT_EQ (keepAlive[0x30], 1);
}

//==============================================================================
// D6, startup.html, "Startup in a channel-specific port": the mixer's
// intention pre-empts the claim; the player answers with a request straight
// to the mixer, takes the number the mixer assigns, sends a final-stage claim
// for it and, once the mixer finishes, keep-alives.
namespace
{
	const uint32_t mixerIp = ip (192, 168, 8, 2);
	prolink::Identity mixer() { return { 0x21, "DJM-V10", { 2, 0, 0, 0, 0, 2 }, mixerIp }; }
}

TEST (ProLinkAnnouncer, AChannelPortAssignsTheNumber)
{
	prolink::Announcer announcer (us());
	typesSent (announcer, 0.0, 1.0);   // hellos, the first first-stage claim

	const auto request = hear (announcer, prolink::mixerIntention (mixer()), mixerIp, 1.0);
	ASSERT_EQ (request.size(), 1u);
	EXPECT_EQ (request[0].to, mixerIp) << "straight to the mixer";
	EXPECT_EQ (request[0].bytes[0x0a], 0x02);
	EXPECT_EQ (request[0].bytes[0x0b], 0x01);
	EXPECT_EQ (request[0].bytes[0x2e], 0x00);

	EXPECT_TRUE (typesSent (announcer, 1.0, 1.2).empty()) << "the own claim waits for the mixer";

	hear (announcer, prolink::numberAssignment (mixer(), 2, 1), mixerIp, 1.2);
	EXPECT_EQ (announcer.device(), 2);
	const auto finalClaim = announcer.due (1.2);
	ASSERT_EQ (finalClaim.size(), 1u);
	EXPECT_EQ (finalClaim[0].bytes[0x0a], 0x04);
	EXPECT_EQ (finalClaim[0].bytes[0x24], 2) << "the assigned number";
	EXPECT_EQ (finalClaim[0].bytes[0x25], 1) << "N";

	hear (announcer, prolink::assignmentFinished (mixer()), mixerIp, 1.21);
	const auto next = announcer.due (1.5);
	ASSERT_EQ (next.size(), 1u);
	EXPECT_EQ (next[0].bytes[0x0a], 0x06) << "keep-alives";
	EXPECT_EQ (next[0].bytes[0x24], 2);
	EXPECT_TRUE (announcer.isOnline());
}

TEST (ProLinkAnnouncer, AnAssignmentNobodyAskedForIsIgnored)
{
	prolink::Announcer announcer (us());
	typesSent (announcer, 0.0, 1.0);
	hear (announcer, prolink::numberAssignment (mixer(), 2, 1), mixerIp, 1.0);
	EXPECT_EQ (announcer.device(), 6);
}

// A mixer that announces and then never assigns: the own claim resumes after
// a second rather than waiting for ever.
TEST (ProLinkAnnouncer, AMixerThatNeverAssignsIsNotWaitedFor)
{
	prolink::Announcer announcer (us());
	typesSent (announcer, 0.0, 1.0);
	hear (announcer, prolink::mixerIntention (mixer()), mixerIp, 1.0);
	EXPECT_TRUE (typesSent (announcer, 1.0, 1.9).empty());
	const auto resumed = typesSent (announcer, 2.0, 6.0);
	ASSERT_FALSE (resumed.empty());
	EXPECT_EQ (resumed.back(), 0x06);
	EXPECT_EQ (announcer.device(), 6);
	EXPECT_TRUE (announcer.isOnline());
}

TEST (ProLinkAnnouncer, ASettledPlayerIgnoresAMixersIntention)
{
	prolink::Announcer announcer (us());
	typesSent (announcer, 0.0, 4.0);
	ASSERT_TRUE (announcer.isOnline());
	EXPECT_TRUE (hear (announcer, prolink::mixerIntention (mixer()), mixerIp, 4.0).empty());
	EXPECT_EQ (announcer.device(), 6);
}
