// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "DeckInstance.h"
#include "DeckLink.h"
#include "OneDeckAudio.h"
#include "OneDeckSync.h"
#include "OneDeckView.h"
#include "ProLinkPackets.h"
#include "ProLinkReceiver.h"
#include "ProLinkSender.h"
#include "TwoDeckFixture.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
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

// Three distinct free UDP ports, as the kernel hands them out (held open
// together, so none comes twice).
std::array<int, 3> freePorts()
{
	std::array<int, 3> fds {}, ports {};
	for (size_t i = 0; i < fds.size(); ++i)
	{
		fds[i] = socket (AF_INET, SOCK_DGRAM, 0);
		sockaddr_in address {};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
		socklen_t length = sizeof (address);
		bind (fds[i], reinterpret_cast<sockaddr*> (&address), sizeof (address));
		getsockname (fds[i], reinterpret_cast<sockaddr*> (&address), &length);
		ports[i] = ntohs (address.sin_port);
	}
	for (auto fd : fds)
		close (fd);
	return ports;
}

// Ports of their own, not the truth's 50000-50002: a link on this machine
// stays untouched. Picked free per test binary run, so two ctest runs at once
// do not hear each other's packets (CTest's RESOURCE_LOCK covers one run).
osctruth::ProLinkPorts testPorts()
{
	static const auto ports = []
	{
		const auto [announce, beat, status] = freePorts();
		osctruth::ProLinkPorts free;
		free.announce = announce;
		free.beat = beat;
		free.status = status;
		return free;
	}();
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

// P4: pinning an interface must not blank the keep-alive's MAC (a detected
// interface fills it); the announce port carries it to every listener.
TEST (ProLinkOnOneHost, APinnedInterfaceStillAnnouncesItsMac)
{
	const auto fd = socket (AF_INET, SOCK_DGRAM, 0);
	ASSERT_GE (fd, 0);
	const int on = 1;
	setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on));
	sockaddr_in address {};
	address.sin_family = AF_INET;
	address.sin_port = htons ((uint16_t) testPorts().announce);
	address.sin_addr.s_addr = htonl (INADDR_ANY);
	ASSERT_EQ (bind (fd, reinterpret_cast<sockaddr*> (&address), sizeof (address)), 0);
	timeval timeout { 1, 0 };
	setsockopt (fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof (timeout));

	const std::array<uint8_t, 6> mac { 0x02, 0xa3, 0x00, 0x00, 0x08, 0x20 };
	ProLinkReceiver receiver;
	receiver.useInterface (loopback, loopbackBroadcast, mac);
	ASSERT_TRUE (receiver.start (deckA, testPorts())) << receiver.error();

	uint8_t buffer[600];
	std::array<uint8_t, 6> heardMac {};
	for (int tries = 0; tries < 5; ++tries)
	{
		const auto n = recv (fd, buffer, sizeof (buffer), 0);
		if (n >= 0x2c && buffer[0x0a] == 0x06)   // keep-alive
		{
			std::copy (buffer + 0x26, buffer + 0x2c, heardMac.begin());
			break;
		}
	}
	close (fd);
	receiver.stop();
	EXPECT_EQ (heardMac, mac);
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

// The deck's audio is heard `pathLatency` after its read head, so its beats
// must line up as heard: a read head 18 ms past a beat is exactly on the
// leader's beat when 18 ms are still in flight.
TEST_F (OneDeckSyncTest, FollowerAlignsItsAudiblePosition)
{
	ASSERT_TRUE (folder.createDirectory().wasOk());
	formats.registerBasicFormats();
	StemSet set;
	set.name = "noise";
	for (int s = 0; s < StemDeckPlayer::numStems; ++s)
	{
		set.files[(size_t) s] = folder.getChildFile ("stem" + juce::String (s) + ".wav");
		twodeck::writeNoiseStem (set.files[(size_t) s], 3 + s);
	}
	ASSERT_EQ (player().load (set), juce::String());
	player().setBeatGrid ({ 120.0, 0.0 });   // load() clears the grid
	player().setPosition (1.018);   // 2 beats at 120 BPM, plus 18 ms
	player().play();

	view.deckPanel().setSyncAvailable (true);
	view.deckPanel().setSyncEnabled (true);
	sync.setSynced (true);
	sync.setPathLatency (0.018);

	hearMaster (120.0, 10.0);   // the leader is on a beat now
	sync.update (10.0);
	EXPECT_NEAR (player().getEffectiveRate(), 1.0, 1e-3) << "audibly on the beat: no nudge";
	EXPECT_NEAR (player().getPosition(), 1.018, 1e-3) << "and no jump";

	sync.setPathLatency (0.0);
	sync.update (10.0);
	EXPECT_LT (player().getEffectiveRate(), 0.99) << "without the offset the read head runs ahead and is held back";
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

// Ruling: MASTER on a deck without a grid is refused -- it could not publish,
// and a second lit MASTER would contradict "one master". With a grid it works.
TEST_F (OneDeckSyncTest, MasterWithoutAGridIsRefused)
{
	player().setBeatGrid ({});
	sync.pressMaster (9.0);
	EXPECT_FALSE (sync.isMaster());
	EXPECT_FALSE (view.deckPanel().isMasterLit());
	EXPECT_FALSE (sync.wantsLink()) << "no claim, no link";

	player().setBeatGrid ({ 120.0, 0.0 });
	sync.pressMaster (10.0);
	EXPECT_TRUE (sync.isMaster());
	EXPECT_TRUE (view.deckPanel().isMasterLit());
}

// S4.6b: two decks on one host each take their own deck 3D, on the truth's
// stemdeck.osc port + (deck - 1). Loopback only; ports picked free at run time.
namespace
{
bool bindable (int port)
{
	const auto fd = socket (AF_INET, SOCK_DGRAM, 0);
	sockaddr_in address {};
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl (INADDR_ANY);
	address.sin_port = htons ((uint16_t) port);
	const auto ok = bind (fd, reinterpret_cast<sockaddr*> (&address), sizeof (address)) == 0;
	close (fd);
	return ok;
}

// A base port whose successor is free too, below the kernel's ephemeral range
// (32768+): the instances' own sockets then cannot take the second port.
int freePairBase()
{
	for (int port = 20000 + (int) (::getpid() % 5000) * 2; port < 32000; port += 2)
		if (bindable (port) && bindable (port + 1))
			return port;
	return -1;
}

std::string writeTruth (int coreOsc, int mixerOsc, int stemdeckOsc)
{
	const auto path = std::filesystem::temp_directory_path() / ("stemdeck-decklink-" + std::to_string (::getpid()) + ".json");
	std::ofstream (path) << R"({"hosts": {"core": "127.0.0.1", "mixer": "127.0.0.1"},
		"listeners": [
			{"program": "core", "role": "osc", "host": "any", "port": )" << coreOsc << R"(},
			{"program": "mixer", "role": "osc", "host": "mixer", "port": )" << mixerOsc << R"(},
			{"program": "stemdeck", "role": "osc", "host": "any", "port": )" << stemdeckOsc << R"(}],
		"addresses": {
			"stemdeck.bus": {"pattern": "/stemdeck/{deck}/{stem}/bus/{bus}"},
			"stemdeck.buses": {"pattern": "/stemdeck/{deck}/{stem}/buses"},
			"stemdeck.recall": {"pattern": "/stemdeck/recall"},
			"vu": {"pattern": "/vu/{n}"},
			"device.hello": {"pattern": "/device/hello"},
			"stemdeck.3d": {"pattern": "/stemdeck/{deck}/3d"},
			"stemdeck.deck-hello": {"pattern": "/stemdeck/{deck}/hello"}}})";
	return path.string();
}

void sendThreeD (int port, const char* address, float value)
{
	juce::OSCSender sender;
	ASSERT_TRUE (sender.connect ("127.0.0.1", port));
	ASSERT_TRUE (sender.send (juce::OSCMessage (juce::String (address), value)));
}

void pump (int milliseconds)
{
	juce::MessageManager::getInstance()->runDispatchLoopUntil (milliseconds);
}
}

TEST (DeckLinkOnOneHost, TwoDecksBindDistinctPortsAndEachTakesItsOwnThreeD)
{
	const auto [core, mixer, unused] = freePorts();
	(void) unused;
	const auto base = freePairBase();
	ASSERT_GT (base, 0);
	const auto truth = writeTruth (core, mixer, base);
	std::atomic<float> threeDA { 1.0f }, threeDB { 1.0f };
	{
		DeckLink deckA (1, threeDA), deckB (2, threeDB);
		deckA.start (truth, "hash");
		deckB.start (truth, "hash");
		pump (100);   // JUCE's receiver threads take a moment to read: a datagram sent at once can be lost

		sendThreeD (base + 1, "/stemdeck/2/3d", 0.25f);
		pump (200);
		EXPECT_FLOAT_EQ (threeDB.load(), 0.25f);
		EXPECT_FLOAT_EQ (threeDA.load(), 1.0f);

		sendThreeD (base, "/stemdeck/1/3d", 0.5f);
		pump (200);
		EXPECT_FLOAT_EQ (threeDA.load(), 0.5f);
		EXPECT_FLOAT_EQ (threeDB.load(), 0.25f);
	}
	std::filesystem::remove (truth);
}
