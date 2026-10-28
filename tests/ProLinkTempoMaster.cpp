// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "ProLinkTempoMaster.h"

// The tempo master role as a CDJ-3000 passes it on, after sync.html of the DJ
// Link analysis ("Tempo Master Handoff", "Unsolicited Handoff") and the
// CDJ-3000X manual p. 101: master moves when MASTER is pressed on another
// player, or when the master pauses or changes track. Pure: the test is the
// clock and the other players.
namespace
{
	constexpr int us = 5;

	prolink::StatusPacket player (int device, bool master, bool playing = true, bool synced = false,
								  int handoffTo = 0xff, uint32_t syncCounter = 0)
	{
		prolink::StatusPacket s;
		s.device = device;
		s.isMaster = master;
		s.isPlaying = playing;
		s.isSynced = synced;
		s.handoffTo = handoffTo;
		s.syncCounter = syncCounter;
		s.bpm = 124.0;
		return s;
	}

	prolink::Control control (prolink::Control::Type type, int device, int value = 0)
	{
		return { type, device, value };
	}

	// Every 200 ms, as a CDJ sends status, from `from` until `until`.
	void hearEvery200ms (prolink::TempoMaster& m, const prolink::StatusPacket& s, double from, double until)
	{
		for (double t = from; t < until; t += 0.2)
			m.hear (s, t);
	}

	prolink::TempoMaster settledAlone()
	{
		prolink::TempoMaster m (us, "StemDeck B", 0.0);
		m.setLocal (true, true, false);
		m.want (true, 0.0);
		m.tick (prolink::TempoMaster::settle);
		return m;
	}

	using Change = prolink::TempoMaster::Change;
	using Type = prolink::Control::Type;
}

// sync.html: "If there is currently no tempo master, the device simply becomes
// master" -- once it has listened long enough to know there is none.
TEST (ProLinkTempoMaster, AloneItBecomesMasterOnceItHasListened)
{
	prolink::TempoMaster m (us, "StemDeck B", 0.0);
	m.setLocal (true, true, false);
	m.want (true, 0.0);
	EXPECT_TRUE (m.tick (0.1).empty());
	EXPECT_FALSE (m.isMaster()) << "a running master may not have been heard yet";
	m.tick (0.6);
	EXPECT_TRUE (m.isMaster());
	EXPECT_EQ (m.state().handoffTo, 0xff);
	EXPECT_EQ (m.takeChange(), Change::none) << "the DJ asked for it";
}

// sync.html: with a master on the link, the takeover request goes to it; the
// role passes when the master names us in Mh.
TEST (ProLinkTempoMaster, ARunningMasterIsAskedAndHandsOver)
{
	prolink::TempoMaster m (us, "StemDeck B", 0.0);
	m.setLocal (true, true, true);
	hearEvery200ms (m, player (2, true), 0.0, 0.6);
	m.want (true, 0.5);
	const auto sent = m.tick (0.6);
	ASSERT_EQ (sent.size(), 1u);
	EXPECT_EQ (sent[0].to, 2);
	EXPECT_EQ (sent[0].bytes[0x0a], 0x26) << "takeover request";
	EXPECT_EQ (sent[0].bytes[0x21], us);
	EXPECT_FALSE (m.isMaster());
	EXPECT_TRUE (m.tick (0.7).empty()) << "asked once";

	m.hear (control (Type::masterResponse, 2, 1), 0.65);
	EXPECT_FALSE (m.isMaster()) << "the response alone does not pass it";
	m.hear (player (2, true, true, false, us), 0.8);
	EXPECT_TRUE (m.isMaster()) << "Mh names us";
	EXPECT_EQ (m.takeChange(), Change::none);

	m.hear (player (2, false), 1.0);
	EXPECT_TRUE (m.isMaster());
}

// No answer from the master (an older StemDeck, a device that ignores the
// request): after a second the role is taken anyway, as StemDeck did before.
TEST (ProLinkTempoMaster, AMasterThatDoesNotAnswerIsOvertaken)
{
	prolink::TempoMaster m (us, "StemDeck B", 0.0);
	m.setLocal (true, true, false);
	hearEvery200ms (m, player (2, true), 0.0, 0.6);
	m.want (true, 0.5);
	m.tick (0.6);
	hearEvery200ms (m, player (2, true), 0.6, 1.5);
	m.tick (1.5);
	EXPECT_FALSE (m.isMaster());
	m.tick (1.65);
	EXPECT_TRUE (m.isMaster());
	hearEvery200ms (m, player (2, true), 1.7, 2.5);
	EXPECT_TRUE (m.isMaster()) << "its claim is older than ours: it is the one to yield";
	EXPECT_EQ (m.takeChange(), Change::none);
}

// sync.html: asked by another player, the master answers, names it in Mh and
// stays master until the new one says it is; then it lets go and sets Syncn
// one above the highest it has seen.
TEST (ProLinkTempoMaster, AskedItHandsOver)
{
	auto m = settledAlone();
	ASSERT_TRUE (m.isMaster());
	m.hear (player (3, false, true, true, 0xff, 7), 1.0);

	const auto replies = m.hear (control (Type::masterRequest, 3), 1.1);
	ASSERT_EQ (replies.size(), 1u);
	EXPECT_EQ (replies[0].to, 3);
	EXPECT_EQ (replies[0].bytes[0x0a], 0x27) << "takeover response";
	EXPECT_EQ (replies[0].bytes[0x21], us);
	EXPECT_TRUE (m.isMaster()) << "still master while the handoff runs";
	EXPECT_EQ (m.state().handoffTo, 3);

	m.hear (player (3, true, true, true, 0xff, 7), 1.3);
	EXPECT_FALSE (m.isMaster());
	EXPECT_EQ (m.state().handoffTo, 0xff);
	EXPECT_EQ (m.state().syncCounter, 8u);
	EXPECT_EQ (m.takeChange(), Change::yielded);
	EXPECT_EQ (m.takeChange(), Change::none) << "told once";
}

TEST (ProLinkTempoMaster, ARequestToANonMasterIsIgnored)
{
	prolink::TempoMaster m (us, "StemDeck B", 0.0);
	EXPECT_TRUE (m.hear (control (Type::masterRequest, 3), 1.0).empty());
	EXPECT_EQ (m.state().handoffTo, 0xff);
}

// Manual p. 101: pausing the sync master (or changing its track) changes the
// master; sync.html's unsolicited handoff names a synced, playing player.
TEST (ProLinkTempoMaster, APausedMasterHandsToASyncedPlayingPlayer)
{
	auto m = settledAlone();
	m.hear (player (2, false, true, true), 1.0);
	m.tick (1.0);
	EXPECT_EQ (m.state().handoffTo, 0xff) << "playing: it stays";

	m.setLocal (true, false, false);
	m.tick (1.1);
	EXPECT_EQ (m.state().handoffTo, 2);
	EXPECT_TRUE (m.isMaster());

	m.hear (player (2, true, true, true), 1.2);
	EXPECT_FALSE (m.isMaster());
	EXPECT_EQ (m.takeChange(), Change::yielded);
}

TEST (ProLinkTempoMaster, APausedMasterKeepsItWithNobodyToTakeIt)
{
	auto m = settledAlone();
	m.setLocal (true, false, false);
	m.hear (player (2, false, true, false), 1.0);   // playing, not synced
	m.hear (player (3, false, false, true), 1.0);   // synced, paused
	m.tick (1.1);
	EXPECT_TRUE (m.isMaster());
	EXPECT_EQ (m.state().handoffTo, 0xff);
}

// The track changed (no grid while it loads): handed over like a pause.
TEST (ProLinkTempoMaster, AMasterWithoutATrackHandsOver)
{
	auto m = settledAlone();
	m.setLocal (false, false, false);
	m.hear (player (2, false, true, true), 1.0);
	m.tick (1.1);
	EXPECT_EQ (m.state().handoffTo, 2);
}

// A handoff whose target goes quiet is withdrawn.
TEST (ProLinkTempoMaster, AHandoffToAPlayerThatGoesQuietIsWithdrawn)
{
	auto m = settledAlone();
	m.setLocal (true, false, false);
	m.hear (player (2, false, true, true), 1.0);
	m.tick (1.1);
	ASSERT_EQ (m.state().handoffTo, 2);
	m.tick (3.5);
	EXPECT_EQ (m.state().handoffTo, 0xff);
	EXPECT_TRUE (m.isMaster());
}

// sync.html's unsolicited handoff, received: named in Mh by the master,
// StemDeck takes the role without having asked -- and says so, so the deck's
// MASTER lights.
TEST (ProLinkTempoMaster, NamedInMhItTakesOverUnasked)
{
	prolink::TempoMaster m (us, "StemDeck B", 0.0);
	m.setLocal (true, true, true);
	m.hear (player (2, true, false, false), 1.0);
	EXPECT_FALSE (m.isMaster());
	m.hear (player (2, true, false, false, us), 1.2);
	EXPECT_TRUE (m.isMaster());
	EXPECT_EQ (m.takeChange(), Change::granted);
}

// A player that claims master without the handoff (one that predates it)
// after StemDeck became master: the newer claim wins, as before.
TEST (ProLinkTempoMaster, ANewerClaimWithoutHandoffWins)
{
	auto m = settledAlone();
	m.hear (player (4, false), 1.0);
	m.hear (player (4, true), 2.0);
	EXPECT_FALSE (m.isMaster());
	EXPECT_EQ (m.takeChange(), Change::yielded);
}

// MASTER pressed again on the master: it gives the role up (manual p. 101).
TEST (ProLinkTempoMaster, TurnedOffItGivesUp)
{
	auto m = settledAlone();
	m.hear (player (2, false, true, false, 0xff, 3), 1.0);
	m.want (false, 1.1);
	EXPECT_FALSE (m.isMaster());
	EXPECT_EQ (m.state().syncCounter, 4u);
	EXPECT_EQ (m.takeChange(), Change::none) << "the DJ did it";
}

// sync.html, "Tempo Master Assignment": the sync control packet with S 01
// (a DJM's touch screen) acts as pressing MASTER here.
TEST (ProLinkTempoMaster, TheBecomeMasterCommandIsAPress)
{
	prolink::TempoMaster m (us, "StemDeck B", 0.0);
	m.setLocal (true, true, false);
	m.hear (control (Type::syncCommand, 0x21, prolink::Control::becomeMaster), 0.1);
	m.tick (0.6);
	EXPECT_TRUE (m.isMaster());
	EXPECT_EQ (m.takeChange(), Change::granted);

	prolink::TempoMaster ignored (us, "StemDeck B", 0.0);
	ignored.hear (control (Type::syncCommand, 0x21, prolink::Control::syncOn), 0.1);
	ignored.tick (0.6);
	EXPECT_FALSE (ignored.isMaster());
}

// D6: the number can change while running (a mixer assigned one).
TEST (ProLinkTempoMaster, ANewNumberIsUsedFromThenOn)
{
	prolink::TempoMaster m (us, "StemDeck B", 0.0);
	m.setDevice (2);
	m.hear (player (3, true, true, false, 2), 1.0);
	EXPECT_TRUE (m.isMaster()) << "Mh names the new number";
}

// Our own status, echoed back (broadcast), is not another player.
TEST (ProLinkTempoMaster, ItsOwnStatusIsNotAnotherPlayer)
{
	auto m = settledAlone();
	m.hear (player (us, true), 2.0);
	EXPECT_TRUE (m.isMaster());
	EXPECT_EQ (m.takeChange(), Change::none);
}

// Review round: without a track there is nothing to lead with -- MASTER is
// not taken, and a DJM's command waits for one.
TEST (ProLinkTempoMaster, WithoutATrackMasterIsNotTaken)
{
	prolink::TempoMaster m (us, "StemDeck B", 0.0);
	m.setLocal (false, false, false);
	m.want (true, 0.0);
	m.tick (0.6);
	EXPECT_FALSE (m.isMaster());

	hearEvery200ms (m, player (2, true), 0.6, 1.0);
	EXPECT_TRUE (m.tick (1.0).empty()) << "nobody is asked either";
	m.setLocal (true, false, false);
	EXPECT_EQ (m.tick (1.1).size(), 1u) << "a track: now it asks";
}

// A master that has agreed (its response) but is slow to name us in Mh gets
// longer before it is overtaken -- two masters are worse than a wait.
TEST (ProLinkTempoMaster, AnAgreedHandoffIsWaitedForLonger)
{
	prolink::TempoMaster m (us, "StemDeck B", 0.0);
	m.setLocal (true, true, false);
	hearEvery200ms (m, player (2, true), 0.0, 0.6);
	m.want (true, 0.5);
	m.tick (0.6);
	m.hear (control (Type::masterResponse, 2, 1), 0.65);
	hearEvery200ms (m, player (2, true), 0.6, 2.0);
	m.tick (2.0);
	EXPECT_FALSE (m.isMaster()) << "agreed 1.35 s ago: still waiting";
	m.tick (0.65 + prolink::TempoMaster::agreedWait);
	EXPECT_TRUE (m.isMaster());
}

// A requester that never takes the role it asked for: Mh is cleared again.
TEST (ProLinkTempoMaster, AnAskedHandoffNotTakenIsWithdrawn)
{
	auto m = settledAlone();
	m.hear (control (Type::masterRequest, 3), 1.0);
	ASSERT_EQ (m.state().handoffTo, 3);
	hearEvery200ms (m, player (3, false, true, true), 1.0, 5.0);
	m.tick (1.0 + prolink::TempoMaster::agreedWait - 0.1);
	EXPECT_EQ (m.state().handoffTo, 3);
	m.tick (1.0 + prolink::TempoMaster::agreedWait + 0.1);
	EXPECT_EQ (m.state().handoffTo, 0xff);
	EXPECT_TRUE (m.isMaster());
}

// Two players taking master at once, each before hearing the other (within a
// status interval): exactly one keeps it -- the lower number, an assumption
// in lieu of a capture -- instead of both letting go.
TEST (ProLinkTempoMaster, ClaimsAtOnceLeaveOneMaster)
{
	auto lower = settledAlone();               // player 5, master since 0.5
	lower.hear (player (6, true), 0.6);        // 6 took it at about the same time
	EXPECT_TRUE (lower.isMaster());

	prolink::TempoMaster higher (6, "StemDeck A", 0.0);
	higher.setLocal (true, true, false);
	higher.want (true, 0.0);
	higher.tick (0.5);
	higher.hear (player (us, true), 0.6);
	EXPECT_FALSE (higher.isMaster());
	EXPECT_EQ (higher.takeChange(), Change::yielded);
}
