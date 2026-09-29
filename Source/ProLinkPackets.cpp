#include "ProLinkPackets.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace prolink
{
	namespace
	{
		const uint8_t magic[10] = { 'Q', 's', 'p', 't', '1', 'W', 'm', 'J', 'O', 'L' };

		constexpr uint8_t typeBeat = 0x28, typeStatus = 0x0a, typeKeepAlive = 0x06;
		constexpr size_t beatSize = 0x60, statusMinSize = 0xa0, keepAliveSize = 0x36;

		constexpr size_t offName = 0x0b, offDevice = 0x21;
		constexpr size_t offPitch = 0x55, offBpm = 0x5a, offBeatInBar = 0x5c;       // beat
		constexpr size_t offFlags = 0x89, offStatusBpm = 0x92;                       // status
		constexpr uint8_t flagMaster = 0x20, flagPlaying = 0x40;
		constexpr uint32_t neutralPitch = 0x100000;
		constexpr uint16_t noTrack = 0xffff;

		uint16_t readBE16 (const uint8_t* p) { return (uint16_t) ((p[0] << 8) | p[1]); }
		uint32_t readBE24 (const uint8_t* p) { return ((uint32_t) p[0] << 16) | ((uint32_t) p[1] << 8) | p[2]; }

		bool isPacket (const uint8_t* data, size_t size, uint8_t type, size_t minSize)
		{
			return data != nullptr && size >= minSize && std::memcmp (data, magic, sizeof (magic)) == 0 && data[0x0a] == type;
		}
	}

	std::optional<BeatPacket> parseBeat (const uint8_t* data, size_t size)
	{
		if (! isPacket (data, size, typeBeat, beatSize))
			return std::nullopt;

		const auto rawBpm = readBE16 (data + offBpm);
		if (rawBpm == noTrack)
			return std::nullopt;

		BeatPacket beat;
		beat.device = data[offDevice];
		beat.trackBpm = rawBpm / 100.0;
		beat.pitch = (double) readBE24 (data + offPitch) / neutralPitch;
		beat.effectiveBpm = beat.trackBpm * beat.pitch;
		beat.beatInBar = data[offBeatInBar];
		if (beat.beatInBar < 1 || beat.beatInBar > 4)
			beat.beatInBar = 1;
		return beat;
	}

	std::optional<StatusPacket> parseStatus (const uint8_t* data, size_t size)
	{
		if (! isPacket (data, size, typeStatus, statusMinSize))
			return std::nullopt;

		StatusPacket status;
		status.device = data[offDevice];
		status.isMaster = (data[offFlags] & flagMaster) != 0;
		status.isPlaying = (data[offFlags] & flagPlaying) != 0;
		const auto rawBpm = readBE16 (data + offStatusBpm);
		status.bpm = rawBpm == noTrack ? 0.0 : rawBpm / 100.0;
		return status;
	}

	namespace
	{
		constexpr size_t statusSize = 0xd4;

		void writeBE16 (uint8_t* p, uint32_t v) { p[0] = (uint8_t) (v >> 8); p[1] = (uint8_t) v; }
		void writeBE24 (uint8_t* p, uint32_t v) { p[0] = (uint8_t) (v >> 16); p[1] = (uint8_t) (v >> 8); p[2] = (uint8_t) v; }

		uint32_t bpmField (double bpm)
		{
			// 0xffff means "no track", so a real tempo stops one short of it.
			return (uint32_t) std::clamp (std::lround (bpm * 100.0), 0L, (long) noTrack - 1);
		}

		std::vector<uint8_t> packet (size_t size, uint8_t type, int device, const std::string& name)
		{
			std::vector<uint8_t> p (size, 0);
			std::memcpy (p.data(), magic, sizeof (magic));
			p[0x0a] = type;
			std::memcpy (p.data() + offName, name.data(), std::min<size_t> (name.size(), 20));
			p[offDevice] = (uint8_t) device;
			return p;
		}
	}

	std::vector<uint8_t> beatPacket (int device, const std::string& name, double bpm, double pitch, int beatInBar)
	{
		auto p = packet (beatSize, typeBeat, device, name);
		writeBE24 (p.data() + offPitch, (uint32_t) std::lround (pitch * neutralPitch));
		writeBE16 (p.data() + offBpm, bpmField (bpm));
		p[offBeatInBar] = (uint8_t) beatInBar;
		return p;
	}

	std::vector<uint8_t> statusPacket (int device, const std::string& name, double bpm, double pitch,
									   bool master, bool playing, uint32_t beatNumber, int beatInBar)
	{
		// Beyond what beat-analyzer reads, the fields prolink-connect reads too,
		// so tools built on it show StemDeck like a CDJ.
		constexpr size_t offPlayState = 0x7b, offSliderPitch = 0x8d, offEffectivePitch = 0x99;
		constexpr size_t offBeatNumber = 0xa0, offStatusBeatInBar = 0xa6;
		constexpr uint8_t statePlaying = 0x03, statePaused = 0x05;

		auto p = packet (statusSize, typeStatus, device, name);
		p[offPlayState] = playing ? statePlaying : statePaused;
		p[offFlags] = (uint8_t) ((master ? flagMaster : 0) | (playing ? flagPlaying : 0));
		const auto pitchField = (uint32_t) std::lround (pitch * neutralPitch);
		writeBE24 (p.data() + offSliderPitch, pitchField);
		writeBE16 (p.data() + offStatusBpm, bpmField (bpm));
		writeBE24 (p.data() + offEffectivePitch, pitchField);
		p[offBeatNumber] = (uint8_t) (beatNumber >> 24);
		p[offBeatNumber + 1] = (uint8_t) (beatNumber >> 16);
		p[offBeatNumber + 2] = (uint8_t) (beatNumber >> 8);
		p[offBeatNumber + 3] = (uint8_t) beatNumber;
		p[offStatusBeatInBar] = (uint8_t) beatInBar;
		return p;
	}

	std::vector<uint8_t> keepAlive (int deviceNumber, const std::string& name,
									const std::array<uint8_t, 6>& mac, uint32_t ipNetworkOrder)
	{
		// 54 bytes, the layout prolink-connect announces with (after the
		// dysentery analysis). beat-analyzer builds it one byte off -- name at
		// 0x0b, length 0x11 -- which it never notices, since it reads none.
		constexpr uint8_t deviceTypeCdj = 0x01;
		std::vector<uint8_t> k (keepAliveSize, 0);
		std::memcpy (k.data(), magic, sizeof (magic));
		k[0x0a] = typeKeepAlive;
		k[0x0b] = 0x00;
		std::memcpy (k.data() + 0x0c, name.data(), std::min<size_t> (name.size(), 20));
		k[0x20] = 0x01;
		k[0x21] = 0x02;
		k[0x22] = 0x00;
		k[0x23] = (uint8_t) keepAliveSize;   // the packet's own length
		k[0x24] = (uint8_t) deviceNumber;
		k[0x25] = deviceTypeCdj;
		std::memcpy (k.data() + 0x26, mac.data(), mac.size());
		std::memcpy (k.data() + 0x2c, &ipNetworkOrder, 4);
		k[0x30] = 0x01;
		k[0x34] = deviceTypeCdj;
		return k;
	}
}
