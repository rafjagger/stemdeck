#pragma once

#include "ProLinkPackets.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <variant>
#include <vector>

// StemDeck on the Pioneer network: a virtual CDJ that listens for beat and
// status packets and announces itself every 1.5 s. Ported from beat-analyzer's
// PioneerReceiver (src/osc/pioneer_receiver.cpp there): one thread, poll()
// over three UDP sockets. SO_REUSEADDR, as beat-analyzer does, so both can
// run on one machine and both get the broadcast beats.
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
	// the ports cannot be bound or no network interface is up.
	bool start (int deviceNumber, const std::string& name = "StemDeck");
	void stop();
	bool isRunning() const { return running.load(); }
	std::string error() const;

	// The subnet's broadcast address the virtual CDJ announces to, in network
	// byte order; 0 until start() has found an interface.
	uint32_t broadcastAddress() const { return broadcastIp; }

	// Everything received since the last call, oldest first.
	std::vector<Event> drain();

private:
	void run();
	void sendKeepAlive();
	bool openSocket (int& fd, int port);
	bool detectInterface();

	std::atomic<bool> running { false };
	std::thread thread;
	int sockAnnounce = -1, sockBeat = -1, sockStatus = -1;
	int deviceNumber = 6;
	std::string name;
	std::array<uint8_t, 6> mac {};
	uint32_t localIp = 0, broadcastIp = 0;

	mutable std::mutex lock;
	std::vector<Event> events;
	std::string lastError;
};
