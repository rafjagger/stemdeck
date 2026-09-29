#include <gtest/gtest.h>

#include "ProLinkPackets.h"

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

TEST (ProLinkPackets, TheKeepAliveMatchesTheLayout)
{
	const std::array<uint8_t, 6> mac { 1, 2, 3, 4, 5, 6 };
	const uint32_t ip = 0x0a08a8c0; // 192.168.8.10 in network order on little-endian
	const auto k = prolink::keepAlive (6, "StemDeck", mac, ip);
	ASSERT_EQ (k.size(), 0x36u);
	EXPECT_EQ (std::memcmp (k.data(), magic, 10), 0);
	EXPECT_EQ (k[0x0a], 0x06);
	EXPECT_EQ (std::string ((const char*) k.data() + 0x0b, 8), "StemDeck");
	EXPECT_EQ (k[0x1f], 0x01);
	EXPECT_EQ (k[0x20], 0x02);
	EXPECT_EQ (k[0x23], 0x11);
	EXPECT_EQ (k[0x24], 6);
	EXPECT_EQ (std::memcmp (k.data() + 0x26, mac.data(), 6), 0);
	EXPECT_EQ (std::memcmp (k.data() + 0x2c, &ip, 4), 0);
}
