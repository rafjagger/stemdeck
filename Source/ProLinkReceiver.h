#pragma once

#include "OscTruth.h"
#include "ProLinkAnnouncer.h"
#include "ProLinkPackets.h"

#include <atomic>
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
// The thread only parses and queues; the message thread drains the queue.
// Each event carries the time it arrived, so a busy UI does not shift beats.
class ProLinkReceiver
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
	bool isOnline() const { return online.load(); }
	std::string error() const;

	// The subnet's broadcast address the player announces to, in network
	// byte order; 0 until start() has found an interface.
	uint32_t broadcastAddress() const { return broadcastIp; }

	// Everything received since the last call, oldest first.
	std::vector<Event> drain();

private:
	void run();
	void announce (const std::vector<prolink::Announcer::Send>& packets);
	void hearAnnouncement (double now);
	bool openSocket (int& fd, int port);

	std::atomic<bool> running { false }, online { false };
	std::thread thread;
	int sockAnnounce = -1, sockBeat = -1, sockStatus = -1;
	osctruth::ProLinkPorts ports;
	int deviceNumber = 6;
	std::string name;
	uint32_t broadcastIp = 0;
	std::unique_ptr<prolink::Announcer> announcer;   // the thread's only
	std::array<uint8_t, 6> forcedMac {};
	uint32_t forcedLocalIp = 0, forcedBroadcastIp = 0;   // useInterface(); 0: find one

	mutable std::mutex lock;
	std::vector<Event> events;
	std::string lastError;
};
