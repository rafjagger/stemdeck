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
		// The layout beat-link's VirtualCdj takes the tempo master with
		// (subtype 04, 11c bytes); a CDJ-3000's own is 200 bytes, with blocks
		// nobody has decoded.
		constexpr size_t statusSize = 0x11c;
		constexpr uint8_t statusSubtype = 0x04;

		void writeBE16 (uint8_t* p, uint32_t v) { p[0] = (uint8_t) (v >> 8); p[1] = (uint8_t) v; }
		void writeBE32 (uint8_t* p, uint32_t v)
		{
			p[0] = (uint8_t) (v >> 24); p[1] = (uint8_t) (v >> 16); p[2] = (uint8_t) (v >> 8); p[3] = (uint8_t) v;
		}

		uint32_t bpmField (double bpm)
		{
			// 0xffff means "no track", so a real tempo stops one short of it.
			return (uint32_t) std::clamp (std::lround (bpm * 100.0), 0L, (long) noTrack - 1);
		}

		uint32_t pitchField (double pitch) { return (uint32_t) std::lround (std::max (0.0, pitch) * neutralPitch); }

		// Ports 50001/50002: the name from 0b, then 01 and the subtype; D and
		// the length of the rest (lenr) follow.
		std::vector<uint8_t> packet (size_t size, uint8_t type, uint8_t subtype, int device, const std::string& name)
		{
			std::vector<uint8_t> p (size, 0);
			std::memcpy (p.data(), magic, sizeof (magic));
			p[0x0a] = type;
			std::memcpy (p.data() + offName, name.data(), std::min<size_t> (name.size(), 20));
			p[0x1f] = 0x01;
			p[0x20] = subtype;
			p[offDevice] = (uint8_t) device;
			writeBE16 (p.data() + 0x22, (uint32_t) (size - 0x24));
			return p;
		}
	}

	std::vector<uint8_t> beatPacket (int device, const std::string& name, double bpm, double pitch, int beatInBar)
	{
		// The coming beats and bars in ms, as at pitch 0 % (beats.html).
		const auto beat = [bpm] (double beats) { return (uint32_t) std::lround (beats * 60000.0 / std::max (bpm, 1.0)); };
		const auto toNextBar = 5 - std::clamp (beatInBar, 1, 4);

		auto p = packet (beatSize, typeBeat, 0x00, device, name);
		writeBE32 (p.data() + 0x24, beat (1));
		writeBE32 (p.data() + 0x28, beat (2));
		writeBE32 (p.data() + 0x2c, beat (toNextBar));
		writeBE32 (p.data() + 0x30, beat (4));
		writeBE32 (p.data() + 0x34, beat (toNextBar + 4));
		writeBE32 (p.data() + 0x38, beat (8));
		std::fill (p.begin() + 0x3c, p.begin() + 0x54, 0xff);
		writeBE32 (p.data() + 0x54, pitchField (pitch));
		writeBE16 (p.data() + offBpm, bpmField (bpm));
		p[offBeatInBar] = (uint8_t) beatInBar;
		p[0x5f] = (uint8_t) device;
		return p;
	}

	std::vector<uint8_t> statusPacket (int device, const std::string& name, double bpm, double pitch,
									   bool master, bool playing, uint32_t beatNumber, int beatInBar)
	{
		// Field names as vcdj.html labels them.
		constexpr uint8_t slotUsb = 0x03, trackRekordbox = 0x01;
		constexpr uint8_t flagsFixed = 0x84;   // bits 7 and 2 are always set

		auto p = packet (statusSize, typeStatus, statusSubtype, device, name);
		p[0x24] = (uint8_t) device;
		p[0x26] = 0x01;
		p[0x27] = playing ? 0x01 : 0x00;   // A
		// Dr, Sr, Tr: an analysed track loaded here -- players accept a
		// master's tempo only from one (Mv below).
		p[0x28] = (uint8_t) device;
		p[0x29] = slotUsb;
		p[0x2a] = trackRekordbox;
		p[0x2f] = 0x01;                    // rekordbox id
		p[0x33] = 0x01;                    // track number
		p[0x6f] = 0x04;                    // Ul: no USB stick
		p[0x73] = 0x04;                    // Sl: no SD card
		p[0x7b] = playing ? 0x03 : 0x05;   // P1
		std::memcpy (p.data() + 0x7c, "1.43", 4);   // Firmware, as beat-link sends it
		p[offFlags] = (uint8_t) (flagsFixed | (master ? flagMaster : 0) | (playing ? flagPlaying : 0));
		p[0x8b] = playing ? 0x7a : 0x7e;   // P2
		// Pitch1/3 the pitch in effect; Pitch2/4 the local fader, frozen at
		// -100 % (0) while paused.
		writeBE32 (p.data() + 0x8c, pitchField (pitch));
		writeBE16 (p.data() + 0x90, 0x8000);   // Mv
		writeBE16 (p.data() + offStatusBpm, bpmField (bpm));
		writeBE16 (p.data() + 0x94, 0x7fff);   // Mslip
		writeBE16 (p.data() + 0x96, 0xffff);   // BPMslip
		writeBE32 (p.data() + 0x98, playing ? pitchField (pitch) : 0);
		p[0x9d] = playing ? 0x09 : 0x01;   // P3: forward, jog in vinyl mode
		p[0x9e] = master ? 0x01 : 0x00;    // Mm
		p[0x9f] = 0xff;                    // Mh: no handoff
		writeBE32 (p.data() + 0xa0, beatNumber);
		writeBE16 (p.data() + 0xa4, 0x01ff);   // Cue: none ahead
		p[0xa6] = (uint8_t) beatInBar;
		writeBE32 (p.data() + 0xc0, pitchField (pitch));
		writeBE32 (p.data() + 0xc4, playing ? pitchField (pitch) : 0);
		p[0xcc] = 0x0f;                    // nx: the layout's family (nexus 2)
		return p;
	}

	//==========================================================================
	namespace
	{
		constexpr uint8_t typeHello = 0x0a, typeClaim1 = 0x00, typeClaim2 = 0x02, typeClaim3 = 0x04;
		constexpr uint8_t typeFinished = 0x05, typeConflict = 0x08;
		constexpr uint8_t aPlayer = 0x01;
		// Byte 21: the old structure is 02; a CDJ-3000 on player 5 or 6 needs
		// 04 in the hello and 03 in the claims.
		constexpr uint8_t structure = 0x02, structureHello3000 = 0x04, structureClaim3000 = 0x03;

		// Port 50000: the name from 0c, 01 at 20, the structure at 21 and the
		// whole packet's length (lenp) at 22.
		std::vector<uint8_t> announcement (size_t size, uint8_t type, uint8_t structureByte, const std::string& name)
		{
			std::vector<uint8_t> p (size, 0);
			std::memcpy (p.data(), magic, sizeof (magic));
			p[0x0a] = type;
			std::memcpy (p.data() + 0x0c, name.data(), std::min<size_t> (name.size(), 20));
			p[0x20] = 0x01;
			p[0x21] = structureByte;
			writeBE16 (p.data() + 0x22, (uint32_t) size);
			return p;
		}

		uint32_t readIp (const uint8_t* p)
		{
			uint32_t ip = 0;
			std::memcpy (&ip, p, 4);
			return ip;
		}
	}

	std::vector<uint8_t> hello (const Identity& self)
	{
		auto p = announcement (0x26, typeHello, structureHello3000, self.name);
		p[0x24] = aPlayer;
		p[0x25] = 0x40;
		return p;
	}

	std::vector<uint8_t> claimStage1 (const Identity& self, int counter)
	{
		auto p = announcement (0x2c, typeClaim1, structureClaim3000, self.name);
		p[0x24] = (uint8_t) counter;
		p[0x25] = aPlayer;
		std::memcpy (p.data() + 0x26, self.mac.data(), 6);
		return p;
	}

	std::vector<uint8_t> claimStage2 (const Identity& self, int counter)
	{
		constexpr uint8_t specificNumber = 0x02;
		auto p = announcement (0x32, typeClaim2, structureClaim3000, self.name);
		std::memcpy (p.data() + 0x24, &self.ip, 4);
		std::memcpy (p.data() + 0x28, self.mac.data(), 6);
		p[0x2e] = (uint8_t) self.device;
		p[0x2f] = (uint8_t) counter;
		p[0x30] = 0x01;
		p[0x31] = specificNumber;
		return p;
	}

	std::vector<uint8_t> claimStage3 (const Identity& self, int counter)
	{
		auto p = announcement (0x26, typeClaim3, structureClaim3000, self.name);
		p[0x24] = (uint8_t) self.device;
		p[0x25] = (uint8_t) counter;
		return p;
	}

	std::vector<uint8_t> keepAlive (const Identity& self, int peers, bool firstOnNetwork)
	{
		constexpr uint8_t cdj3000 = 0x64;
		auto k = announcement (keepAliveSize, typeKeepAlive, structure, self.name);
		k[0x24] = (uint8_t) self.device;
		k[0x25] = firstOnNetwork ? 0x02 : 0x01;
		std::memcpy (k.data() + 0x26, self.mac.data(), 6);
		std::memcpy (k.data() + 0x2c, &self.ip, 4);
		k[0x30] = (uint8_t) std::clamp (peers, 1, 255);
		k[0x34] = aPlayer;
		k[0x35] = cdj3000;
		return k;
	}

	std::vector<uint8_t> conflict (const Identity& self)
	{
		auto p = announcement (0x29, typeConflict, structure, self.name);
		p[0x24] = (uint8_t) self.device;
		std::memcpy (p.data() + 0x25, &self.ip, 4);
		return p;
	}

	std::vector<uint8_t> assignmentFinished (const Identity& self)
	{
		auto p = announcement (0x26, typeFinished, structure, self.name);
		p[0x24] = (uint8_t) self.device;
		p[0x25] = 0x01;
		return p;
	}

	std::optional<Announcement> parseAnnouncement (const uint8_t* data, size_t size)
	{
		constexpr size_t headerSize = 0x24;
		if (data == nullptr || size < headerSize || std::memcmp (data, magic, sizeof (magic)) != 0)
			return std::nullopt;

		using Type = Announcement::Type;
		const auto type = data[0x0a];
		const auto fits = [size] (size_t needed) { return size >= needed; };

		if (type == typeKeepAlive && fits (keepAliveSize))
			return Announcement { Type::keepAlive, data[0x24], readIp (data + 0x2c) };
		if (type == typeClaim2 && fits (0x32))
			return Announcement { Type::claim, data[0x2e], readIp (data + 0x24) };
		if (type == typeClaim3 && fits (0x26))
			return Announcement { Type::finalClaim, data[0x24], 0 };
		if (type == typeConflict && fits (0x29))
			return Announcement { Type::conflict, data[0x24], readIp (data + 0x25) };
		if (type == typeFinished && fits (0x26))
			return Announcement { Type::assignmentFinished, data[0x24], 0 };
		if (type == typeHello || type == typeClaim1 || type == 0x01 || type == 0x03)
			return Announcement { Type::other, 0, 0 };
		return std::nullopt;
	}
}
