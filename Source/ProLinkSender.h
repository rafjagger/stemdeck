#pragma once

#include "OscTruth.h"
#include "ProLinkPackets.h"

#include <atomic>
#include <string>
#include <thread>

class StemDeckPlayer;

// What the sender needs from the player on the link (ProLinkReceiver): whether
// its number is claimed, which number, the tempo master role -- and where it
// learns what the reported deck is doing. Called on the sender's thread.
class ProLinkRole
{
public:
	virtual ~ProLinkRole() = default;
	virtual bool isOnline() const = 0;
	virtual int device() const = 0;
	virtual prolink::MasterState masterState() const = 0;
	virtual void reportLocal (bool hasTrack, bool playing, bool synced) = 0;
};

// StemDeck as a player on the link, as a CDJ-3000: a status every 200 ms
// whenever on the link (master or not, track or not), the absolute position
// every 30 ms while a track is loaded, and the MASTER deck's beats -- as
// broadcast, so beat-analyzer's clock mode 2 hears it even on the same
// machine (unicast would reach only the socket bound last; a3-system #70).
// Its own thread, not the UI timer: a beat goes out within a millisecond of
// the read head crossing it, from the deck's position carried to "now"
// (positionAt).
//
// The players are read through their atomics only; setMaster()/setReported()
// swap them from the message thread, and stop() must run before a player
// goes away.
class ProLinkSender
{
public:
	~ProLinkSender();

	// False when the truth has no ports for it or no socket opens. With a
	// `role`, nothing goes out while it is not online -- a CDJ sends no status
	// or beat before its player number is claimed -- and the status carries
	// its number and master role. Without one (tests): `deviceNumber`, and
	// master while a MASTER deck is set.
	bool start (int deviceNumber, uint32_t broadcastIpNetworkOrder,
				const osctruth::ProLinkPorts& ports, const std::string& name = "StemDeck",
				ProLinkRole* role = nullptr);
	void stop();
	bool isRunning() const { return running.load(); }

	// The deck whose beat goes out, or nullptr for none. It is also the deck
	// the status and position report.
	void setMaster (const StemDeckPlayer* player) { master = player; }
	// The deck the status and position report while no deck is MASTER, or
	// nullptr: then they report no track.
	void setReported (const StemDeckPlayer* player) { reported = player; }
	// SYNC on the reported deck (the status's Sync flag).
	void setSynced (bool on) { synced = on; }

	// Beats go out when they are heard, not when the read head crosses them.
	void setPathLatency (double seconds) { pathLatency = seconds; }

private:
	void run();
	void send (const std::vector<uint8_t>& packet, int port);
	int device() const { return role != nullptr ? role->device() : deviceNumber; }

	std::atomic<bool> running { false }, synced { false };
	std::atomic<const StemDeckPlayer*> master { nullptr }, reported { nullptr };
	std::atomic<double> pathLatency { 0.0 };
	std::thread thread;
	int socketFd = -1;
	osctruth::ProLinkPorts ports;
	int deviceNumber = 6;
	uint32_t broadcastIp = 0;
	std::string name;
	ProLinkRole* role = nullptr;
};
