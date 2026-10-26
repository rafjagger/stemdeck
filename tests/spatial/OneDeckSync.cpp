// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "DeckInstance.h"
#include "OneDeckAudio.h"
#include "OneDeckSync.h"
#include "OneDeckView.h"
#include "ProLinkPackets.h"
#include "ProLinkReceiver.h"
#include "ProLinkSender.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <thread>

// S4.7: decks in separate instances (-bformat --deck A / B) stay in sync over
// the link. Sockets only on loopback: 127.255.255.255 is lo's broadcast
// address, so the test sees what two instances on one host see, and nothing
// leaves the machine.
namespace
{
constexpr int deckA = 6, deckB = 8;   // DeckInstance::proLinkDevice()
const uint32_t loopback = htonl (INADDR_LOOPBACK);
const uint32_t loopbackBroadcast = htonl (0x7fffffff);   // 127.255.255.255

// Ports of their own, not the truth's 50000-50002: a link on this machine stays untouched.
osctruth::ProLinkPorts testPorts()
{
	osctruth::ProLinkPorts ports;
	ports.announce = 47810;
	ports.beat = 47811;
	ports.status = 47812;
	return ports;
}

bool startOnLoopback (ProLinkReceiver& receiver, int device)
{
	receiver.useInterface (loopback, loopbackBroadcast);
	return receiver.start (device, testPorts());
}

void broadcast (const std::vector<uint8_t>& packet, int port)
{
	const auto fd = socket (AF_INET, SOCK_DGRAM, 0);
	ASSERT_GE (fd, 0);
	const int on = 1;
	setsockopt (fd, SOL_SOCKET, SO_BROADCAST, &on, sizeof (on));
	sockaddr_in to {};
	to.sin_family = AF_INET;
	to.sin_port = htons ((uint16_t) port);
	to.sin_addr.s_addr = loopbackBroadcast;
	ASSERT_EQ (sendto (fd, packet.data(), packet.size(), 0, reinterpret_cast<sockaddr*> (&to), sizeof (to)), (ssize_t) packet.size());
	close (fd);
}

// What `receiver` heard within `seconds`, beats by sender.
template <typename Packet>
std::vector<Packet> heard (ProLinkReceiver& receiver, double seconds)
{
	std::vector<Packet> packets;
	const auto until = std::chrono::steady_clock::now() + std::chrono::duration<double> (seconds);
	while (std::chrono::steady_clock::now() < until)
	{
		for (auto& event : receiver.drain())
			if (const auto* packet = std::get_if<Packet> (&event.packet))
				packets.push_back (*packet);
		std::this_thread::sleep_for (std::chrono::milliseconds (10));
	}
	return packets;
}
}

// The finding's evidence: two receivers on the same three ports on one host
// both hear a beat broadcast; each drops only its own number.
TEST (ProLinkOnOneHost, TwoInstancesHearOneAnothersBeat)
{
	ProLinkReceiver a, b;
	ASSERT_TRUE (startOnLoopback (a, deckA)) << a.error();
	ASSERT_TRUE (startOnLoopback (b, deckB)) << b.error() << " -- the second instance must bind the same ports";

	broadcast (prolink::beatPacket (deckB, "StemDeck", 124.0, 1.0, 1), testPorts().beat);
	broadcast (prolink::beatPacket (7, "beat-analyzer", 124.0, 1.0, 2), testPorts().beat);

	const auto atA = heard<prolink::BeatPacket> (a, 0.3);
	const auto atB = heard<prolink::BeatPacket> (b, 0.3);
	ASSERT_EQ (atA.size(), 2u) << "deck A hears deck B and the third player";
	EXPECT_EQ (atA[0].device, deckB);
	EXPECT_NEAR (atA[0].effectiveBpm, 124.0, 0.01);
	ASSERT_EQ (atB.size(), 1u) << "deck B drops its own beat, hears the third player";
	EXPECT_EQ (atB[0].device, 7);
}

// ...and the real sender of the MASTER instance reaches both listeners: its
// status says master, with its tempo.
TEST (ProLinkOnOneHost, TheMasterInstancesSenderReachesBothListeners)
{
	juce::ScopedJuceInitialiser_GUI juce;
	juce::AudioFormatManager formats;
	StemDeckPlayer player (formats);
	player.setBeatGrid ({ 126.0, 0.0 });

	ProLinkReceiver a, other;
	ASSERT_TRUE (startOnLoopback (a, deckA)) << a.error();
	ASSERT_TRUE (startOnLoopback (other, 7)) << other.error();

	ProLinkSender sender;
	sender.setMaster (&player);
	ASSERT_TRUE (sender.start (deckB, loopbackBroadcast, testPorts()));

	for (auto* receiver : { &a, &other })
	{
		const auto statuses = heard<prolink::StatusPacket> (*receiver, 0.5);
		ASSERT_FALSE (statuses.empty());
		EXPECT_EQ (statuses.front().device, deckB);
		EXPECT_TRUE (statuses.front().isMaster);
		EXPECT_NEAR (statuses.front().bpm, 126.0, 0.01);
	}
	sender.stop();
}

//==============================================================================
// The one-deck screen's SYNC and MASTER, without sockets: the link's events in,
// the deck's tempo and lights out.
namespace
{
class OneDeckSyncTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		player().setBeatGrid ({ 120.0, 0.0 });
		view.setSize (768, 700);
	}

	StemDeckPlayer& player() { return audio.host().player(); }

	// Deck B is master and plays at `bpm`; its beat at `seconds`.
	void hearMaster (double bpm, double seconds)
	{
		prolink::StatusPacket status;
		status.device = deckB;
		status.isMaster = status.isPlaying = true;
		status.bpm = bpm;
		prolink::BeatPacket beat;
		beat.device = deckB;
		beat.trackBpm = beat.effectiveBpm = bpm;
		sync.hear ({ { status, seconds - 0.01 }, { beat, seconds } });
	}

	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	juce::File folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("stemdeck-sync", "");
	juce::AudioFormatManager formats;
	juce::ThreadPool pool;
	AnalysisCache cache { folder.getChildFile ("analysis.xml") };
	OneDeckAudio audio { formats, pool, cache };
	OneDeckView view { audio, formats, 0 };
	OneDeckSync sync { player(), view.deckPanel() };
};
}

TEST_F (OneDeckSyncTest, SyncFollowsTheOtherInstancesMaster)
{
	EXPECT_FALSE (sync.wantsLink()) << "off the link until SYNC or MASTER";
	view.deckPanel().setSyncAvailable (true);   // as the owner does with the link's ports
	view.deckPanel().setSyncEnabled (true);     // the key pressed ...
	sync.setSynced (true);                      // ... and its onSyncToggled
	EXPECT_TRUE (sync.wantsLink());

	hearMaster (126.0, 10.0);
	sync.update (10.05);
	EXPECT_NEAR (player().getSpeed(), 126.0 / 120.0, 1e-6);
	EXPECT_TRUE (view.deckPanel().isSyncEnabled()) << "a tempo set by sync is not a hand on the fader";
}

TEST_F (OneDeckSyncTest, WithNoMasterAnywhereTheDeckKeepsItsOwnGrid)
{
	sync.setSynced (true);
	sync.update (10.0);
	EXPECT_DOUBLE_EQ (player().getSpeed(), 1.0);
}

TEST_F (OneDeckSyncTest, TheMasterDeckLeadsAndDoesNotFollow)
{
	sync.setSynced (true);
	sync.pressMaster (9.0);
	EXPECT_TRUE (sync.isMaster());
	EXPECT_TRUE (view.deckPanel().isMasterLit());

	prolink::BeatPacket beat;
	beat.device = 7;
	beat.trackBpm = beat.effectiveBpm = 130.0;
	sync.hear ({ { beat, 10.0 } });
	sync.update (10.05);
	EXPECT_DOUBLE_EQ (player().getSpeed(), 1.0);
}

TEST_F (OneDeckSyncTest, AnotherInstanceTakingMasterTurnsThisOneOff)
{
	sync.joined (5.0);
	sync.pressMaster (9.0);
	hearMaster (126.0, 10.0);   // deck B pressed MASTER after this one
	EXPECT_TRUE (sync.update (10.05)) << "gave master up";
	EXPECT_FALSE (sync.isMaster());
	EXPECT_FALSE (view.deckPanel().isMasterLit());
}

TEST_F (OneDeckSyncTest, SyncOffReleasesTheNudge)
{
	sync.setSynced (true);
	player().setSyncNudge (1.01);
	sync.setSynced (false);
	EXPECT_DOUBLE_EQ (player().getEffectiveRate(), 1.0);
	EXPECT_FALSE (sync.wantsLink());
}

// The S4.4b minor: a session restores SYNC lit only where SYNC can work. With
// the keys unavailable (no link ports in the truth) it stays dark; available,
// it comes back lit.
TEST_F (OneDeckSyncTest, RestoredSyncIsLitOnlyWhereSyncIsAvailable)
{
	DeckSession session;
	session.sync = true;

	view.deckPanel().setSyncAvailable (false);
	view.deckPanel().restoreState (session);
	EXPECT_FALSE (view.deckPanel().isSyncEnabled());

	view.deckPanel().setSyncAvailable (true);
	view.deckPanel().restoreState (session);
	EXPECT_TRUE (view.deckPanel().isSyncEnabled());
}
