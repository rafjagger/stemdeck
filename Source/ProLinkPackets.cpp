#include "ProLinkPackets.h"

#include <algorithm>
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

	std::vector<uint8_t> keepAlive (int deviceNumber, const std::string& name,
									const std::array<uint8_t, 6>& mac, uint32_t ipNetworkOrder)
	{
		// 54 bytes, as beat-analyzer builds it (buildKeepAlivePacket there).
		std::vector<uint8_t> k (keepAliveSize, 0);
		std::memcpy (k.data(), magic, sizeof (magic));
		k[0x0a] = typeKeepAlive;
		std::memcpy (k.data() + offName, name.data(), std::min<size_t> (name.size(), 20));
		k[0x1f] = 0x01;
		k[0x20] = 0x02;   // subtype: keep-alive
		k[0x23] = 0x11;   // bytes remaining
		k[0x24] = (uint8_t) deviceNumber;
		std::memcpy (k.data() + 0x26, mac.data(), mac.size());
		std::memcpy (k.data() + 0x2c, &ipNetworkOrder, 4);
		return k;
	}
}
