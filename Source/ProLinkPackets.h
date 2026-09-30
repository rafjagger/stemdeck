#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Pioneer Pro DJ Link, the parts StemDeck needs to follow the tempo master:
// beat packets, CDJ status packets and the keep-alive a virtual CDJ sends.
// Layout as beat-analyzer reads it (include/osc/pioneer_receiver.h there),
// after the dysentery protocol analysis. Pure: no sockets, no JUCE.
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
	// (port 50001) and a status packet every 200 ms (port 50002), carrying
	// every field beat-analyzer reads. `pitch` is the fader ratio, `bpm` the
	// track's own; the effective tempo is their product.
	std::vector<uint8_t> beatPacket (int device, const std::string& name, double bpm, double pitch, int beatInBar);
	std::vector<uint8_t> statusPacket (int device, const std::string& name, double bpm, double pitch,
									   bool master, bool playing, uint32_t beatNumber, int beatInBar);

	// The keep-alive a virtual CDJ announces itself with, every 1.5 s on port 50000.
	std::vector<uint8_t> keepAlive (int deviceNumber, const std::string& name,
									const std::array<uint8_t, 6>& mac, uint32_t ipNetworkOrder);
}
