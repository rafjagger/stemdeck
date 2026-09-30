#pragma once

#include "OscTruth.h"
#include "ProLinkPackets.h"

#include <atomic>
#include <string>
#include <thread>

class StemDeckPlayer;

// StemDeck as the Pioneer tempo master: sends the master deck's beats and a
// status every 200 ms, as broadcast -- so beat-analyzer's clock mode 2 hears
// it even on the same machine (unicast would reach only the socket bound
// last; a3-system #70). Its own thread, not the UI timer: a beat goes out
// within a millisecond of the read head crossing it, from the deck's position
// carried to "now" (positionAt).
//
// The player is read through its atomics only; setMaster() swaps it from the
// message thread, and stop() must run before the player goes away.
class ProLinkSender
{
public:
	~ProLinkSender();

	// False when the truth has no ports for it or no socket opens.
	bool start (int deviceNumber, uint32_t broadcastIpNetworkOrder,
				const osctruth::ProLinkPorts& ports, const std::string& name = "StemDeck");
	void stop();
	bool isRunning() const { return running.load(); }

	// The deck whose beat goes out, or nullptr for none.
	void setMaster (const StemDeckPlayer* player) { master = player; }

private:
	void run();
	void send (const std::vector<uint8_t>& packet, int port);

	std::atomic<bool> running { false };
	std::atomic<const StemDeckPlayer*> master { nullptr };
	std::thread thread;
	int socketFd = -1;
	osctruth::ProLinkPorts ports;
	int deviceNumber = 6;
	uint32_t broadcastIp = 0;
	std::string name;
};
