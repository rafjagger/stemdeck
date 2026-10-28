#pragma once

#include "OscTruth.h"
#include "ProLinkAnnouncer.h"
#include "ProLinkPackets.h"
#include "ProLinkSender.h"
#include "ProLinkTempoMaster.h"

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <variant>
#include <vector>

// StemDeck on the Pro DJ Link network: a player on the link that listens for
// beat and status packets and joins as a CDJ-3000 does -- claims its number,
// then a keep-alive every two seconds (ProLinkAnnouncer). Ported from beat-analyzer's
// PioneerReceiver (src/osc/pioneer_receiver.cpp there): one thread, poll()
// over three UDP sockets. SO_REUSEADDR, as beat-analyzer does, so both can
// run on one machine and both get the broadcast beats -- as do two one-deck
// instances (S4.7; a unicast would reach only the socket bound last).
//
// It also holds the player's tempo master role (prolink::TempoMaster): it
// hears the other players' status and the takeover packets on port 50001,
// and answers them. The sender reads the role (ProLinkRole); the deck's
// MASTER key sets what the DJ wants and follows what the link hands over.
//
// The thread only parses and queues; the message thread drains the queue.
// Each event carries the time it arrived, so a busy UI does not shift beats.
class ProLinkReceiver : public ProLinkRole
{
public:
	struct Event
	{
		std::variant<prolink::BeatPacket, prolink::StatusPacket> packet;
		double seconds = 0.0; // juce::Time::getMillisecondCounterHiRes() / 1000 at receipt
	};

	~ProLinkReceiver();

	// Opens the sockets and starts the thread. False, with error() set, when
	// the truth has no ports for it, the ports cannot be bound or no network
	// interface qualifies (LinkInterface.h: the rule that picks one).
	bool start (int deviceNumber, const osctruth::ProLinkPorts& ports,
				const std::string& name = "StemDeck");
	void stop();
	// Use this interface instead of finding one at start(): the tests' loopback
	// (127.0.0.1, broadcast 127.255.255.255). Network byte order.
	// `mac` goes into the keep-alive; zeros when not given.
	void useInterface (uint32_t localIpNetworkOrder, uint32_t broadcastIpNetworkOrder,
					   const std::array<uint8_t, 6>& mac = {});
	bool isRunning() const { return running.load(); }
	// The player number claimed: what it sends may go out. Running but not
	// online is still claiming, or refused (error() says by whom).
	bool isOnline() const override { return online.load(); }
	std::string error() const;

	// The player number in use: the one started with, or one a mixer's
	// channel port assigned.
	int device() const override { return currentDevice.load(); }

	// The tempo master role, for the sender's status packets.
	prolink::MasterState masterState() const override;
	void reportLocal (bool hasTrack, bool playing, bool synced) override;

	// MASTER as the deck shows it; asked for, or given up, on the link.
	void wantMaster (bool on) { wantsMaster = on; }
	// What the deck's MASTER must follow, once: the role handed here
	// unasked, or taken by another player.
	prolink::TempoMaster::Change takeMasterChange();

	// The subnet's broadcast address the player announces to, in network
	// byte order; 0 until start() has found an interface.
	uint32_t broadcastAddress() const { return broadcastIp; }

	// Everything received since the last call, oldest first.
	std::vector<Event> drain();

private:
	void run();
	void announce (const std::vector<prolink::Announcer::Send>& packets);
	void hearAnnouncement (double now);
	void hearStatus (double now);
	void hearBeatPort (double now);
	void runTempoMaster (double now);
	void sendControl (const std::vector<prolink::TempoMaster::Send>& packets);
	bool openSocket (int& fd, int port);

	std::atomic<bool> running { false }, online { false };
	std::thread thread;
	int sockAnnounce = -1, sockBeat = -1, sockStatus = -1;
	osctruth::ProLinkPorts ports;
	int deviceNumber = 6;
	std::string name;
	uint32_t broadcastIp = 0;
	std::unique_ptr<prolink::Announcer> announcer;   // the thread's only
	std::unique_ptr<prolink::TempoMaster> tempo;     // the thread's only
	std::map<int, uint32_t> addresses;               // device -> where it sends from; the thread's only
	uint32_t ownIp = 0;
	bool lastWantsMaster = false;                    // the thread's only
	std::atomic<int> currentDevice { 6 };
	std::atomic<bool> wantsMaster { false }, localTrack { false }, localPlaying { false }, localSynced { false };
	std::array<uint8_t, 6> forcedMac {};
	uint32_t forcedLocalIp = 0, forcedBroadcastIp = 0;   // useInterface(); 0: find one

	mutable std::mutex lock;
	std::vector<Event> events;
	prolink::MasterState role;
	prolink::TempoMaster::Change pendingChange = prolink::TempoMaster::Change::none;
	std::string lastError;
};
