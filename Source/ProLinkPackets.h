#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Pioneer Pro DJ Link, the parts StemDeck needs to be a player on the link:
// beat packets, CDJ status packets and the port-50000 packets a CDJ-3000 joins
// with. Layouts after Deep Symmetry's DJ Link analysis
// (djl-analysis.deepsymmetry.org). Pure: no sockets, no JUCE.
namespace prolink
{
	// The ports are not here: they come from the one truth, a3-osc.json --
	// see OscTruth.h.

	struct BeatPacket
	{
		int device = 0;           // players 1-4, the mixer is 0x21
		double trackBpm = 0.0;    // as the track is analysed
		double pitch = 1.0;       // 1 = neutral
		double effectiveBpm = 0.0;
		int beatInBar = 1;        // 1-4
	};

	struct StatusPacket
	{
		int device = 0;
		bool isMaster = false;
		bool isPlaying = false;
		double bpm = 0.0;
	};

	// A beat packet, or nothing: wrong magic, wrong type, too short, or no track loaded.
	std::optional<BeatPacket> parseBeat (const uint8_t* data, size_t size);

	// A CDJ status packet, or nothing.
	std::optional<StatusPacket> parseStatus (const uint8_t* data, size_t size);

	// What StemDeck sends as the tempo master: a beat packet on every beat
	// (port 50001) and a status packet every 200 ms (port 50002), with the
	// fields a player reads to follow a master. `pitch` is the fader ratio,
	// `bpm` the track's own; the effective tempo is their product.
	std::vector<uint8_t> beatPacket (int device, const std::string& name, double bpm, double pitch, int beatInBar);
	std::vector<uint8_t> statusPacket (int device, const std::string& name, double bpm, double pitch,
									   bool master, bool playing, uint32_t beatNumber, int beatInBar);

	//==========================================================================
	// Port 50000: how a player joins the link and holds its number, as a
	// CDJ-3000 does (startup.html of the DJ Link analysis). `ip` in network
	// byte order.
	struct Identity
	{
		int device = 0;
		std::string name;
		std::array<uint8_t, 6> mac {};
		uint32_t ip = 0;
	};

	std::vector<uint8_t> hello (const Identity&);
	std::vector<uint8_t> claimStage1 (const Identity&, int counter);
	std::vector<uint8_t> claimStage2 (const Identity&, int counter);
	std::vector<uint8_t> claimStage3 (const Identity&, int counter);
	// `peers` counts the devices on the link, this one included;
	// `firstOnNetwork` whether nobody was there when it started.
	std::vector<uint8_t> keepAlive (const Identity&, int peers, bool firstOnNetwork);
	// "This number is mine", straight to a device claiming it.
	std::vector<uint8_t> conflict (const Identity&);
	// "Your claim is done", straight to a device in its final claim stage.
	std::vector<uint8_t> assignmentFinished (const Identity&);

	struct Announcement
	{
		enum class Type { keepAlive, claim, finalClaim, conflict, assignmentFinished, other };
		Type type = Type::other;
		int device = 0;      // 0 where the packet carries none
		uint32_t ip = 0;     // network byte order; 0 where the packet carries none
	};

	// A port-50000 packet from any device, or nothing when it is not one.
	std::optional<Announcement> parseAnnouncement (const uint8_t* data, size_t size);
}
