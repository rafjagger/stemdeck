#include <gtest/gtest.h>

#include "ProLinkPackets.h"

#include <cmath>
#include <cstring>

namespace
{
	const uint8_t magic[10] = { 'Q', 's', 'p', 't', '1', 'W', 'm', 'J', 'O', 'L' };

	std::vector<uint8_t> beatBytes (int device, int bpmTimes100, uint32_t pitchRaw, int beatInBar)
	{
		std::vector<uint8_t> b (0x60, 0);
		std::memcpy (b.data(), magic, 10);
		b[0x0a] = 0x28;
		b[0x21] = (uint8_t) device;
		b[0x55] = (uint8_t) (pitchRaw >> 16);
		b[0x56] = (uint8_t) (pitchRaw >> 8);
		b[0x57] = (uint8_t) pitchRaw;
		b[0x5a] = (uint8_t) (bpmTimes100 >> 8);
		b[0x5b] = (uint8_t) bpmTimes100;
		b[0x5c] = (uint8_t) beatInBar;
		return b;
	}

	std::vector<uint8_t> statusBytes (int device, uint8_t flags, int bpmTimes100)
	{
		std::vector<uint8_t> b (0xd4, 0);
		std::memcpy (b.data(), magic, 10);
		b[0x0a] = 0x0a;
		b[0x21] = (uint8_t) device;
		b[0x89] = flags;
		b[0x92] = (uint8_t) (bpmTimes100 >> 8);
		b[0x93] = (uint8_t) bpmTimes100;
		return b;
	}
}

TEST (ProLinkPackets, ABeatPacketParses)
{
	const auto b = beatBytes (2, 12800, 0x100000, 3);
	const auto beat = prolink::parseBeat (b.data(), b.size());
	ASSERT_TRUE (beat.has_value());
	EXPECT_EQ (beat->device, 2);
	EXPECT_DOUBLE_EQ (beat->trackBpm, 128.0);
	EXPECT_DOUBLE_EQ (beat->pitch, 1.0);
	EXPECT_DOUBLE_EQ (beat->effectiveBpm, 128.0);
	EXPECT_EQ (beat->beatInBar, 3);
}

TEST (ProLinkPackets, PitchChangesTheEffectiveTempo)
{
	const auto b = beatBytes (1, 12800, (uint32_t) (0x100000 * 1.08), 1);
	const auto beat = prolink::parseBeat (b.data(), b.size());
	ASSERT_TRUE (beat.has_value());
	EXPECT_NEAR (beat->effectiveBpm, 138.24, 0.01);
}

TEST (ProLinkPackets, NoTrackIsNotABeat)
{
	const auto b = beatBytes (1, 0xFFFF, 0x100000, 1);
	EXPECT_FALSE (prolink::parseBeat (b.data(), b.size()).has_value());
}

TEST (ProLinkPackets, WrongMagicTypeOrSizeIsRejected)
{
	auto wrongMagic = beatBytes (1, 12800, 0x100000, 1);
	wrongMagic[0] = 'X';
	auto wrongType = beatBytes (1, 12800, 0x100000, 1);
	wrongType[0x0a] = 0x29;
	const auto full = beatBytes (1, 12800, 0x100000, 1);

	EXPECT_FALSE (prolink::parseBeat (wrongMagic.data(), wrongMagic.size()).has_value());
	EXPECT_FALSE (prolink::parseBeat (wrongType.data(), wrongType.size()).has_value());
	EXPECT_FALSE (prolink::parseBeat (full.data(), 0x50).has_value());
}

TEST (ProLinkPackets, ABeatInBarOutOfRangeIsOne)
{
	const auto b = beatBytes (1, 12800, 0x100000, 7);
	EXPECT_EQ (prolink::parseBeat (b.data(), b.size())->beatInBar, 1);
}

TEST (ProLinkPackets, TheMasterFlagIsRead)
{
	const auto master = statusBytes (3, 0x60, 12400);
	const auto other = statusBytes (2, 0x40, 12400);
	const auto m = prolink::parseStatus (master.data(), master.size());
	const auto o = prolink::parseStatus (other.data(), other.size());
	ASSERT_TRUE (m.has_value());
	ASSERT_TRUE (o.has_value());
	EXPECT_EQ (m->device, 3);
	EXPECT_TRUE (m->isMaster);
	EXPECT_TRUE (m->isPlaying);
	EXPECT_DOUBLE_EQ (m->bpm, 124.0);
	EXPECT_FALSE (o->isMaster);
}

// The layout prolink-connect announces its virtual CDJ with (src/virtualcdj,
// after the dysentery analysis): type 0x06 0x00, the name from 0x0c, 01 02,
// the packet's own length 0x0036, device, device type, MAC, IP, 01 00 00 00,
// device type again.
TEST (ProLinkPackets, TheKeepAliveMatchesTheLayout)
{
	const std::array<uint8_t, 6> mac { 1, 2, 3, 4, 5, 6 };
	const uint32_t ip = 0x0a08a8c0; // 192.168.8.10 in network order on little-endian
	const auto k = prolink::keepAlive (6, "StemDeck", mac, ip);
	ASSERT_EQ (k.size(), 0x36u);
	EXPECT_EQ (std::memcmp (k.data(), magic, 10), 0);
	EXPECT_EQ (k[0x0a], 0x06);
	EXPECT_EQ (k[0x0b], 0x00);
	EXPECT_EQ (std::string ((const char*) k.data() + 0x0c, 8), "StemDeck");
	EXPECT_EQ (k[0x20], 0x01);
	EXPECT_EQ (k[0x21], 0x02);
	EXPECT_EQ (k[0x22], 0x00);
	EXPECT_EQ (k[0x23], 0x36);
	EXPECT_EQ (k[0x24], 6);
	EXPECT_EQ (k[0x25], 0x01) << "device type: CDJ";
	EXPECT_EQ (std::memcmp (k.data() + 0x26, mac.data(), 6), 0);
	EXPECT_EQ (std::memcmp (k.data() + 0x2c, &ip, 4), 0);
	EXPECT_EQ (k[0x30], 0x01);
	EXPECT_EQ (k[0x34], 0x01);
}

// Part 2: StemDeck as the tempo master. What it sends must read back through
// the same parsers beat-analyzer's layout is copied from.
TEST (ProLinkPackets, ABuiltBeatPacketParsesBack)
{
	const auto b = prolink::beatPacket (6, "StemDeck", 125.5, 1.024, 3);
	const auto beat = prolink::parseBeat (b.data(), b.size());
	ASSERT_TRUE (beat.has_value());
	EXPECT_EQ (beat->device, 6);
	EXPECT_DOUBLE_EQ (beat->trackBpm, 125.5);
	EXPECT_NEAR (beat->pitch, 1.024, 1e-6);
	EXPECT_NEAR (beat->effectiveBpm, 128.512, 0.01);
	EXPECT_EQ (beat->beatInBar, 3);
	EXPECT_EQ (std::string ((const char*) b.data() + 0x0b, 8), "StemDeck");
}

TEST (ProLinkPackets, ABuiltStatusPacketParsesBack)
{
	const auto playing = prolink::statusPacket (6, "StemDeck", 128.0, 1.0, true, true, 17, 2);
	const auto stopped = prolink::statusPacket (6, "StemDeck", 128.0, 1.0, true, false, 17, 2);
	const auto p = prolink::parseStatus (playing.data(), playing.size());
	const auto s = prolink::parseStatus (stopped.data(), stopped.size());
	ASSERT_TRUE (p.has_value());
	ASSERT_TRUE (s.has_value());
	EXPECT_EQ (p->device, 6);
	EXPECT_TRUE (p->isMaster);
	EXPECT_TRUE (p->isPlaying);
	EXPECT_DOUBLE_EQ (p->bpm, 128.0);
	EXPECT_TRUE (s->isMaster);
	EXPECT_FALSE (s->isPlaying);
}

TEST (ProLinkPackets, BuiltPacketsHaveTheRightSize)
{
	EXPECT_EQ (prolink::beatPacket (6, "StemDeck", 120.0, 1.0, 1).size(), 0x60u);
	EXPECT_EQ (prolink::statusPacket (6, "StemDeck", 120.0, 1.0, true, true, 1, 1).size(), 0xd4u);
}

// The fields prolink-connect reads from a status packet as well, so tools
// built on it (prolink-tools) see StemDeck like a CDJ.
TEST (ProLinkPackets, TheStatusCarriesWhatProlinkConnectReads)
{
	const auto p = prolink::statusPacket (6, "StemDeck", 125.0, 1.024, true, true, 17, 2);
	EXPECT_EQ (p[0x7b], 0x03) << "play state: playing";
	EXPECT_EQ (p[0x89] & 0x40, 0x40) << "flag: playing";
	const uint32_t pitch = (uint32_t) std::lround (1.024 * 0x100000);
	EXPECT_EQ (p[0x8d], (uint8_t) (pitch >> 16));
	EXPECT_EQ (p[0x8f], (uint8_t) pitch);
	EXPECT_EQ (p[0x99], (uint8_t) (pitch >> 16));
	EXPECT_EQ (p[0xa3], 17) << "beat number, BE32 at 0xa0";
	EXPECT_EQ (p[0xa6], 2) << "beat in bar";
	const auto paused = prolink::statusPacket (6, "StemDeck", 125.0, 1.0, true, false, 17, 2);
	EXPECT_EQ (paused[0x7b], 0x05) << "play state: paused";
}
