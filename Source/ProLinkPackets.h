#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
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
		bool isSynced = false;
		int handoffTo = 0xff;        // Mh: the player master is being handed to; ff none
		uint32_t syncCounter = 0;    // Syncn; a mixer reports none
		bool isMixer = false;
	};

	// A beat packet, or nothing: wrong magic, wrong type, too short, or no track loaded.
	std::optional<BeatPacket> parseBeat (const uint8_t* data, size_t size);

	// A CDJ status packet, or nothing.
	std::optional<StatusPacket> parseStatus (const uint8_t* data, size_t size);
	// A mixer status packet (type 29), or nothing: a DJM can hold master too.
	std::optional<StatusPacket> parseMixerStatus (const uint8_t* data, size_t size);

	// What StemDeck sends as a player: a beat packet on every beat (port
	// 50001) and a status packet every 200 ms (port 50002), with the fields a
	// player reads to follow a master. `pitch` is the fader ratio, `bpm` the
	// track's own; the effective tempo is their product. `msToTrackEnd`, in
	// track time (pitch 0 %): beats and bars after it are ffffffff.
	std::vector<uint8_t> beatPacket (int device, const std::string& name, double bpm, double pitch, int beatInBar,
									 double msToTrackEnd = std::numeric_limits<double>::infinity());

	// The deck a status packet reports. No track (or no grid, so nothing a
	// player could sync to): Tr 0 and BPM ffff, as a CDJ with nothing loaded.
	struct PlayerState
	{
		bool hasTrack = false;
		double bpm = 0.0;
		double pitch = 1.0;
		bool playing = false;
		bool synced = false;
		uint32_t beatNumber = 0;
		int beatInBar = 1;
	};

	// The tempo master role as the status reports it (sync.html).
	struct MasterState
	{
		bool master = false;
		int handoffTo = 0xff;   // Mh
		uint32_t syncCounter = 0;   // Syncn
	};

	std::vector<uint8_t> statusPacket (int device, const std::string& name, const PlayerState&, const MasterState&);
	// A master with a track loaded: the shorthand of the tests and older callers.
	std::vector<uint8_t> statusPacket (int device, const std::string& name, double bpm, double pitch,
									   bool master, bool playing, uint32_t beatNumber, int beatInBar);

	// CDJ-3000 absolute position (type 0b, port 50001, every 30 ms while a
	// track is loaded, paused too; beats.html).
	struct Position
	{
		double trackSeconds = 0.0;
		double playheadSeconds = 0.0;
		double pitch = 1.0;          // the fader ratio
		double effectiveBpm = 0.0;   // 0: unknown
	};
	std::vector<uint8_t> absolutePosition (int device, const std::string& name, const Position&);

	//==========================================================================
	// Port 50001: the tempo master handoff and the sync control packet
	// (sync.html).
	std::vector<uint8_t> masterRequest (int device, const std::string& name);    // to the current master
	std::vector<uint8_t> masterResponse (int device, const std::string& name);   // to the requester

	struct Control
	{
		enum class Type { masterRequest, masterResponse, syncCommand };
		// syncCommand values (S)
		static constexpr int becomeMaster = 0x01, syncOn = 0x10, syncOff = 0x20;

		Type type = Type::masterRequest;
		int device = 0;   // the sender
		int value = 0;    // S of a sync command; 1 in a response
	};
	std::vector<uint8_t> syncCommand (int device, const std::string& name, int value);
	std::optional<Control> parseControl (const uint8_t* data, size_t size);

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

	// A mixer channel port assigns the number (startup.html, "Startup in a
	// channel-specific port"): its intention, the player's request back to
	// it, and the number it assigns. The mixer's two are built for the tests.
	std::vector<uint8_t> mixerIntention (const Identity& mixer);
	std::vector<uint8_t> numberRequest (const Identity&, int counter);
	std::vector<uint8_t> numberAssignment (const Identity& mixer, int device, int counter);

	struct Announcement
	{
		enum class Type { keepAlive, claim, finalClaim, conflict, assignmentFinished, mixerIntention, numberAssigned, other };
		Type type = Type::other;
		int device = 0;      // 0 where the packet carries none
		uint32_t ip = 0;     // network byte order; 0 where the packet carries none
	};

	// A port-50000 packet from any device, or nothing when it is not one.
	std::optional<Announcement> parseAnnouncement (const uint8_t* data, size_t size);
}
