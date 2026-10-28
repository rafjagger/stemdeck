#include <gtest/gtest.h>

#include "ProLinkPackets.h"

#include <array>
#include <cmath>
#include <cstring>

namespace
{
	uint32_t htonlTest (int a, int b, int c, int d)
	{
		const uint8_t bytes[4] { (uint8_t) a, (uint8_t) b, (uint8_t) c, (uint8_t) d };
		uint32_t networkOrder = 0;
		std::memcpy (&networkOrder, bytes, 4);
		return networkOrder;
	}

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
	EXPECT_EQ (prolink::statusPacket (6, "StemDeck", 120.0, 1.0, true, true, 1, 1).size(), 0x11cu);
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

//==============================================================================
// Byte layouts as Deep Symmetry's DJ Link analysis draws them
// (https://djl-analysis.deepsymmetry.org/djl-analysis/), each test citing its
// page. Offsets are hexadecimal, as there.
namespace
{
	const std::array<uint8_t, 6> testMac { 0x02, 0xa3, 0x00, 0x00, 0x08, 0x14 };
	const uint32_t testIp = htonlTest (192, 168, 8, 20);

	uint32_t be32 (const std::vector<uint8_t>& p, size_t at)
	{
		return ((uint32_t) p[at] << 24) | ((uint32_t) p[at + 1] << 16) | ((uint32_t) p[at + 2] << 8) | p[at + 3];
	}

	uint16_t be16 (const std::vector<uint8_t>& p, size_t at) { return (uint16_t) ((p[at] << 8) | p[at + 1]); }

	prolink::Identity deck6() { return { 6, "StemDeck", testMac, testIp }; }

	// The header every port-50000 packet starts with: magic, type, 00, the
	// name from 0c, 01 at 20, the structure byte at 21 and lenp at 22.
	void expectAnnounceHeader (const std::vector<uint8_t>& p, uint8_t type, uint8_t structure)
	{
		ASSERT_GE (p.size(), 0x24u);
		EXPECT_EQ (std::memcmp (p.data(), magic, 10), 0);
		EXPECT_EQ (p[0x0a], type);
		EXPECT_EQ (p[0x0b], 0x00);
		EXPECT_EQ (std::string ((const char*) p.data() + 0x0c, 8), "StemDeck");
		EXPECT_EQ (p[0x14], 0x00) << "the name is padded with 00 to 20 bytes";
		EXPECT_EQ (p[0x20], 0x01);
		EXPECT_EQ (p[0x21], structure);
		EXPECT_EQ (be16 (p, 0x22), p.size()) << "lenp: the whole packet";
	}
}

// startup.html, "CDJ-3000 compatible keep-alive packets": D, 01, MAC, IP,
// then p (peers, itself included) at 30, 01 (a player, not a mixer) at 34
// and 64 at 35 -- without it a CDJ-3000 on player 5 or 6 can drop off the link.
TEST (ProLinkPackets, TheKeepAliveIsACdj3000s)
{
	const auto k = prolink::keepAlive (deck6(), 3, false);
	ASSERT_EQ (k.size(), 0x36u);
	expectAnnounceHeader (k, 0x06, 0x02);
	EXPECT_EQ (k[0x24], 6) << "device number";
	EXPECT_EQ (k[0x25], 0x01) << "another device was on the link first";
	EXPECT_EQ (std::memcmp (k.data() + 0x26, testMac.data(), 6), 0) << "MAC";
	EXPECT_EQ (std::memcmp (k.data() + 0x2c, &testIp, 4), 0) << "IP";
	EXPECT_EQ (k[0x30], 3) << "peers";
	EXPECT_EQ (k[0x31], 0x00);
	EXPECT_EQ (k[0x34], 0x01) << "a player";
	EXPECT_EQ (k[0x35], 0x64) << "CDJ-3000";
}

// startup.html, keep-alive: byte 25 latches 02 when the player was the first
// device present, 01 when another was already there.
TEST (ProLinkPackets, TheKeepAliveSaysWhoWasFirst)
{
	EXPECT_EQ (prolink::keepAlive (deck6(), 1, true)[0x25], 0x02);
	EXPECT_EQ (prolink::keepAlive (deck6(), 1, false)[0x25], 0x01);
}

// startup.html, "CDJ-3000 compatible initial announcement packets": structure
// 04, one byte longer than the old one, payload 01 40.
TEST (ProLinkPackets, TheHelloIsACdj3000s)
{
	const auto h = prolink::hello (deck6());
	ASSERT_EQ (h.size(), 0x26u);
	expectAnnounceHeader (h, 0x0a, 0x04);
	EXPECT_EQ (h[0x24], 0x01);
	EXPECT_EQ (h[0x25], 0x40);
}

// startup.html, CDJ-3000 first-stage claim: structure 03, N at 24, 01 (a
// player) at 25, MAC from 26.
TEST (ProLinkPackets, TheFirstStageClaim)
{
	const auto c = prolink::claimStage1 (deck6(), 2);
	ASSERT_EQ (c.size(), 0x2cu);
	expectAnnounceHeader (c, 0x00, 0x03);
	EXPECT_EQ (c[0x24], 2) << "N";
	EXPECT_EQ (c[0x25], 0x01);
	EXPECT_EQ (std::memcmp (c.data() + 0x26, testMac.data(), 6), 0);
}

// startup.html, CDJ-3000 second-stage claim: IP at 24, MAC at 28, D at 2e,
// N at 2f, 01 at 30, a at 31: 02, a specific number (StemDeck's is fixed per deck).
TEST (ProLinkPackets, TheSecondStageClaim)
{
	const auto c = prolink::claimStage2 (deck6(), 3);
	ASSERT_EQ (c.size(), 0x32u);
	expectAnnounceHeader (c, 0x02, 0x03);
	EXPECT_EQ (std::memcmp (c.data() + 0x24, &testIp, 4), 0);
	EXPECT_EQ (std::memcmp (c.data() + 0x28, testMac.data(), 6), 0);
	EXPECT_EQ (c[0x2e], 6) << "D";
	EXPECT_EQ (c[0x2f], 3) << "N";
	EXPECT_EQ (c[0x30], 0x01);
	EXPECT_EQ (c[0x31], 0x02) << "a: a specific number";
}

// startup.html, CDJ-3000 final-stage claim: D at 24, N at 25.
TEST (ProLinkPackets, TheFinalStageClaim)
{
	const auto c = prolink::claimStage3 (deck6(), 1);
	ASSERT_EQ (c.size(), 0x26u);
	expectAnnounceHeader (c, 0x04, 0x03);
	EXPECT_EQ (c[0x24], 6);
	EXPECT_EQ (c[0x25], 1);
}

// startup.html, "Channel conflict packet": the defender's D and IP.
TEST (ProLinkPackets, TheConflictPacket)
{
	const auto c = prolink::conflict (deck6());
	ASSERT_EQ (c.size(), 0x29u);
	expectAnnounceHeader (c, 0x08, 0x02);
	EXPECT_EQ (c[0x24], 6);
	EXPECT_EQ (std::memcmp (c.data() + 0x25, &testIp, 4), 0);
}

// startup.html, "CDJ device number assignment finished packet": a settled
// player's own D, then 01; it cuts a booting player's final-stage series short.
TEST (ProLinkPackets, TheAssignmentFinishedPacket)
{
	const auto f = prolink::assignmentFinished (deck6());
	ASSERT_EQ (f.size(), 0x26u);
	expectAnnounceHeader (f, 0x05, 0x02);
	EXPECT_EQ (f[0x24], 6);
	EXPECT_EQ (f[0x25], 0x01);
}

// What other devices say on port 50000, read back: the number and, where the
// packet carries it, the address.
TEST (ProLinkPackets, AnnouncementsParse)
{
	auto other = deck6();
	other.device = 3;
	other.ip = htonlTest (192, 168, 8, 33);

	const auto keepAlive = prolink::keepAlive (other, 2, false);
	const auto k = prolink::parseAnnouncement (keepAlive.data(), keepAlive.size());
	ASSERT_TRUE (k);
	EXPECT_EQ (k->type, prolink::Announcement::Type::keepAlive);
	EXPECT_EQ (k->device, 3);
	EXPECT_EQ (k->ip, other.ip);

	const auto stage2 = prolink::claimStage2 (other, 1);
	const auto c2 = prolink::parseAnnouncement (stage2.data(), stage2.size());
	ASSERT_TRUE (c2);
	EXPECT_EQ (c2->type, prolink::Announcement::Type::claim);
	EXPECT_EQ (c2->device, 3);
	EXPECT_EQ (c2->ip, other.ip);

	const auto stage3 = prolink::claimStage3 (other, 1);
	const auto c3 = prolink::parseAnnouncement (stage3.data(), stage3.size());
	ASSERT_TRUE (c3);
	EXPECT_EQ (c3->type, prolink::Announcement::Type::finalClaim);
	EXPECT_EQ (c3->device, 3);

	const auto defended = prolink::conflict (other);
	const auto c = prolink::parseAnnouncement (defended.data(), defended.size());
	ASSERT_TRUE (c);
	EXPECT_EQ (c->type, prolink::Announcement::Type::conflict);
	EXPECT_EQ (c->device, 3);
	EXPECT_EQ (c->ip, other.ip);

	const auto finished = prolink::assignmentFinished (other);
	const auto f = prolink::parseAnnouncement (finished.data(), finished.size());
	ASSERT_TRUE (f);
	EXPECT_EQ (f->type, prolink::Announcement::Type::assignmentFinished);

	const auto hello = prolink::hello (other);
	const auto h = prolink::parseAnnouncement (hello.data(), hello.size());
	ASSERT_TRUE (h);
	EXPECT_EQ (h->type, prolink::Announcement::Type::other);

	EXPECT_FALSE (prolink::parseAnnouncement (keepAlive.data(), 0x20)) << "too short";
	const auto beat = prolink::beatPacket (6, "StemDeck", 120.0, 1.0, 1);
	EXPECT_FALSE (prolink::parseAnnouncement (beat.data(), beat.size())) << "not a port-50000 packet";
}

// beats.html, "Beat packet": name from 0b, 01 at 1f, subtype 00 at 20, D at
// 21 and again at 5f, lenr 003c; the next beats and bars in ms at normal
// speed (pitch 0 %), ff from 3c to 53, pitch BE32 at 54, BPM at 5a, Bb at 5c.
TEST (ProLinkPackets, TheBeatPacketIsACdjs)
{
	const auto b = prolink::beatPacket (6, "StemDeck", 120.0, 1.08, 3);
	ASSERT_EQ (b.size(), 0x60u);
	EXPECT_EQ (b[0x0a], 0x28);
	EXPECT_EQ (std::string ((const char*) b.data() + 0x0b, 8), "StemDeck");
	EXPECT_EQ (b[0x1f], 0x01);
	EXPECT_EQ (b[0x20], 0x00);
	EXPECT_EQ (b[0x21], 6);
	EXPECT_EQ (be16 (b, 0x22), 0x3c) << "lenr: what follows it";
	EXPECT_EQ (be32 (b, 0x24), 500u) << "nextBeat: 120 BPM at pitch 0 %";
	EXPECT_EQ (be32 (b, 0x28), 1000u) << "2ndBeat";
	EXPECT_EQ (be32 (b, 0x2c), 1000u) << "nextBar: from beat 3, two beats";
	EXPECT_EQ (be32 (b, 0x30), 2000u) << "4thBeat";
	EXPECT_EQ (be32 (b, 0x34), 3000u) << "2ndBar";
	EXPECT_EQ (be32 (b, 0x38), 4000u) << "8thBeat";
	for (size_t i = 0x3c; i < 0x54; ++i)
		EXPECT_EQ (b[i], 0xff) << "byte " << std::hex << i;
	EXPECT_EQ (be32 (b, 0x54), (uint32_t) std::lround (1.08 * 0x100000)) << "pitch";
	EXPECT_EQ (be16 (b, 0x5a), 12000) << "the track's BPM";
	EXPECT_EQ (b[0x5c], 3) << "Bb";
	EXPECT_EQ (b[0x5f], 6) << "D again";
}

// vcdj.html, "CDJ status packet", in the layout beat-link's VirtualCdj takes
// the tempo master with (subtype 04, 11c bytes): the fields a player reads to
// follow a master.
TEST (ProLinkPackets, TheStatusPacketIsACdjs)
{
	const auto s = prolink::statusPacket (6, "StemDeck", 125.0, 1.024, true, true, 17, 2);
	ASSERT_EQ (s.size(), 0x11cu);
	EXPECT_EQ (s[0x0a], 0x0a);
	EXPECT_EQ (s[0x1f], 0x01);
	EXPECT_EQ (s[0x20], 0x04);
	EXPECT_EQ (s[0x21], 6);
	EXPECT_EQ (be16 (s, 0x22), 0x11c - 0x24) << "lenr";
	EXPECT_EQ (s[0x24], 6) << "D again";
	EXPECT_EQ (s[0x27], 0x01) << "A: active";
	EXPECT_EQ (s[0x28], 6) << "Dr: loaded here";
	EXPECT_EQ (s[0x2a], 0x01) << "Tr: an analysed track, or players ignore its tempo";
	EXPECT_EQ (s[0x7b], 0x03) << "P1: playing";
	EXPECT_EQ (s[0x89], 0x84 | 0x40 | 0x20) << "F: fixed bits, Play, Master";
	EXPECT_EQ (s[0x8b], 0x7a) << "P2: playing";
	const uint32_t pitch = (uint32_t) std::lround (1.024 * 0x100000);
	EXPECT_EQ (be32 (s, 0x8c), pitch) << "Pitch1";
	EXPECT_EQ (be16 (s, 0x90), 0x8000) << "Mv: tempo accepted from this master";
	EXPECT_EQ (be16 (s, 0x92), 12500) << "BPM";
	EXPECT_EQ (be16 (s, 0x94), 0x7fff) << "Mslip: not slipping";
	EXPECT_EQ (be16 (s, 0x96), 0xffff) << "BPMslip";
	EXPECT_EQ (be32 (s, 0x98), pitch) << "Pitch2";
	EXPECT_EQ (s[0x9d], 0x09) << "P3: playing forward";
	EXPECT_EQ (s[0x9e], 0x01) << "Mm: a meaningful master";
	EXPECT_EQ (s[0x9f], 0xff) << "Mh: no handoff";
	EXPECT_EQ (be32 (s, 0xa0), 17u) << "Beat";
	EXPECT_EQ (be16 (s, 0xa4), 0x01ff) << "Cue: no cue ahead";
	EXPECT_EQ (s[0xa6], 2) << "Bb";
	EXPECT_EQ (be32 (s, 0xc0), pitch) << "Pitch3";
	EXPECT_EQ (be32 (s, 0xc4), pitch) << "Pitch4";
	EXPECT_EQ (be32 (s, 0xc8), 0u) << "Packet: a CDJ-3000 does not count";
}

// vcdj.html: paused, P1 05, P2 7e, P3 01, A 00; Pitch2 and Pitch4 follow the
// local fader frozen at -100 % (000000), Pitch1 and Pitch3 keep the tempo.
TEST (ProLinkPackets, APausedStatus)
{
	const auto s = prolink::statusPacket (6, "StemDeck", 125.0, 1.024, false, false, 17, 2);
	EXPECT_EQ (s[0x27], 0x00);
	EXPECT_EQ (s[0x7b], 0x05);
	EXPECT_EQ (s[0x89], 0x84) << "neither Play nor Master";
	EXPECT_EQ (s[0x8b], 0x7e);
	EXPECT_EQ (s[0x9d], 0x01);
	EXPECT_EQ (s[0x9e], 0x00) << "Mm: not master";
	EXPECT_EQ (be32 (s, 0x8c), (uint32_t) std::lround (1.024 * 0x100000));
	EXPECT_EQ (be32 (s, 0x98), 0u);
	EXPECT_EQ (be32 (s, 0xc4), 0u);
}

//==============================================================================
// D2, vcdj.html: a player with no track loaded still sends status -- Dr, Sr
// and Tr 00, BPM ffff, Mv 7fff, P1 and P3 00, Beat ffffffff, Bb 0, and it is
// never master.
TEST (ProLinkPackets, AStatusWithNoTrack)
{
	prolink::PlayerState idle;   // no track
	const auto s = prolink::statusPacket (6, "StemDeck", idle, {});
	ASSERT_EQ (s.size(), 0x11cu);
	EXPECT_EQ (s[0x27], 0x00) << "A: idle";
	EXPECT_EQ (s[0x28], 0x00) << "Dr";
	EXPECT_EQ (s[0x29], 0x00) << "Sr";
	EXPECT_EQ (s[0x2a], 0x00) << "Tr: no track";
	EXPECT_EQ (be32 (s, 0x2c), 0u) << "rekordbox id";
	EXPECT_EQ (s[0x7b], 0x00) << "P1: no track";
	EXPECT_EQ (s[0x89], 0x84) << "F: neither Play nor Master";
	EXPECT_EQ (be16 (s, 0x90), 0x7fff) << "Mv: no track";
	EXPECT_EQ (be16 (s, 0x92), 0xffff) << "BPM: no track";
	EXPECT_EQ (s[0x9d], 0x00) << "P3: no track";
	EXPECT_EQ (s[0x9e], 0x00) << "Mm";
	EXPECT_EQ (be32 (s, 0xa0), 0xffffffffu) << "Beat: none";
	EXPECT_EQ (s[0xa6], 0x00) << "Bb: none";

	const auto parsed = prolink::parseStatus (s.data(), s.size());
	ASSERT_TRUE (parsed);
	EXPECT_DOUBLE_EQ (parsed->bpm, 0.0);
	EXPECT_FALSE (parsed->isMaster);
}

// sync.html and vcdj.html: Sync is bit 4 of F, Mh at 9f names the player
// master is being handed to, Syncn at 84 counts master changes.
TEST (ProLinkPackets, TheStatusCarriesSyncHandoffAndSyncCounter)
{
	prolink::PlayerState deck;
	deck.hasTrack = true;
	deck.bpm = 124.0;
	deck.playing = true;
	deck.synced = true;
	prolink::MasterState role;
	role.master = true;
	role.handoffTo = 3;
	role.syncCounter = 0x01020304;
	const auto s = prolink::statusPacket (5, "StemDeck B", deck, role);
	EXPECT_EQ (s[0x89], 0x84 | 0x40 | 0x20 | 0x10) << "F: Play, Master, Sync";
	EXPECT_EQ (s[0x9e], 0x01) << "Mm";
	EXPECT_EQ (s[0x9f], 3) << "Mh";
	EXPECT_EQ (be32 (s, 0x84), 0x01020304u) << "Syncn";

	const auto parsed = prolink::parseStatus (s.data(), s.size());
	ASSERT_TRUE (parsed);
	EXPECT_TRUE (parsed->isSynced);
	EXPECT_TRUE (parsed->isMaster);
	EXPECT_EQ (parsed->handoffTo, 3);
	EXPECT_EQ (parsed->syncCounter, 0x01020304u);
	EXPECT_FALSE (parsed->isMixer);

	const auto plain = prolink::statusPacket (5, "StemDeck B", deck, {});
	const auto p = prolink::parseStatus (plain.data(), plain.size());
	ASSERT_TRUE (p);
	EXPECT_EQ (p->handoffTo, 0xff) << "no handoff";
	EXPECT_FALSE (p->isMaster);
}

// vcdj.html, "Mixer status packets" (type 29, 38 bytes): F at 27 (f0 master,
// d0 not), BPM at 2e, Mh at 36. A DJM can hold or hand over master too.
TEST (ProLinkPackets, AMixerStatusParses)
{
	std::vector<uint8_t> m (0x38, 0);
	std::memcpy (m.data(), magic, 10);
	m[0x0a] = 0x29;
	m[0x1f] = 0x01;
	m[0x21] = 0x21;
	m[0x24] = 0x21;
	m[0x27] = 0xf0;
	m[0x2e] = 0x30;   // 0x3070 = 124.00 BPM
	m[0x2f] = 0x70;
	m[0x36] = 0x02;
	const auto parsed = prolink::parseMixerStatus (m.data(), m.size());
	ASSERT_TRUE (parsed);
	EXPECT_EQ (parsed->device, 0x21);
	EXPECT_TRUE (parsed->isMixer);
	EXPECT_TRUE (parsed->isMaster);
	EXPECT_TRUE (parsed->isSynced);
	EXPECT_EQ (parsed->handoffTo, 2);
	EXPECT_DOUBLE_EQ (parsed->bpm, 124.0);

	m[0x27] = 0xd0;
	EXPECT_FALSE (prolink::parseMixerStatus (m.data(), m.size())->isMaster);
	EXPECT_FALSE (prolink::parseMixerStatus (m.data(), 0x30)) << "too short";
	EXPECT_FALSE (prolink::parseStatus (m.data(), m.size())) << "not a CDJ status";
}

// D8, beats.html: a beat or bar the track ends before is reported as
// ffffffff.
TEST (ProLinkPackets, BeatTimingsPastTheTrackEndAreUnknown)
{
	// 120 BPM, beat 3 of the bar: next beat 500 ms, 2nd 1000, next bar 1000,
	// 4th 2000, 2nd bar 3000, 8th 4000. The track ends in 1500 ms.
	const auto b = prolink::beatPacket (6, "StemDeck", 120.0, 1.0, 3, 1500.0);
	EXPECT_EQ (be32 (b, 0x24), 500u);
	EXPECT_EQ (be32 (b, 0x28), 1000u);
	EXPECT_EQ (be32 (b, 0x2c), 1000u);
	EXPECT_EQ (be32 (b, 0x30), 0xffffffffu) << "4thBeat after the end";
	EXPECT_EQ (be32 (b, 0x34), 0xffffffffu) << "2ndBar after the end";
	EXPECT_EQ (be32 (b, 0x38), 0xffffffffu) << "8thBeat after the end";

	const auto atTheEnd = prolink::beatPacket (6, "StemDeck", 120.0, 1.0, 3, 400.0);
	EXPECT_EQ (be32 (atTheEnd, 0x24), 0xffffffffu) << "not even the next beat";

	const auto open = prolink::beatPacket (6, "StemDeck", 120.0, 1.0, 3);
	EXPECT_EQ (be32 (open, 0x38), 4000u) << "no end given: all known";
}

// D9, beats.html, "Absolute Position packets" (CDJ-3000): type 0b on port
// 50001, 02 at 1f, subtype 00, D at 21, lenr 0018; track length in whole
// seconds at 24, playhead in ms at 28, the pitch fader in hundredths of a
// percent (signed) at 2c, the effective BPM times ten at 38.
TEST (ProLinkPackets, TheAbsolutePositionPacket)
{
	prolink::Position position;
	position.trackSeconds = 245.9;
	position.playheadSeconds = 61.2345;
	position.pitch = 1.0326;
	position.effectiveBpm = 120.2;
	const auto p = prolink::absolutePosition (5, "StemDeck B", position);
	ASSERT_EQ (p.size(), 0x3cu);
	EXPECT_EQ (std::memcmp (p.data(), magic, 10), 0);
	EXPECT_EQ (p[0x0a], 0x0b);
	EXPECT_EQ (std::string ((const char*) p.data() + 0x0b, 10), "StemDeck B");
	EXPECT_EQ (p[0x1f], 0x02);
	EXPECT_EQ (p[0x20], 0x00);
	EXPECT_EQ (p[0x21], 5);
	EXPECT_EQ (be16 (p, 0x22), 0x18) << "lenr";
	EXPECT_EQ (be32 (p, 0x24), 245u) << "rounded down";
	EXPECT_EQ (be32 (p, 0x28), 61234u) << "ms";
	EXPECT_EQ (be32 (p, 0x2c), 326u) << "+3.26 %";
	for (size_t i = 0x30; i < 0x38; ++i)
		EXPECT_EQ (p[i], 0x00);
	EXPECT_EQ (be32 (p, 0x38), 1202u);

	position.pitch = 0.95;
	position.effectiveBpm = 0.0;
	const auto slow = prolink::absolutePosition (5, "StemDeck B", position);
	EXPECT_EQ ((int32_t) be32 (slow, 0x2c), -500) << "-5 %";
	EXPECT_EQ (be32 (slow, 0x38), 0xffffffffu) << "unknown BPM";
	EXPECT_FALSE (prolink::parseBeat (p.data(), p.size())) << "not a beat";
}

// D3, sync.html: the takeover request (26, lenr 0004, D at 21 and 27) and its
// response (27, lenr 0008, D at 21 and 27, 01 at 2b), both on port 50001; the
// sync control packet (2a) with S at 2b.
TEST (ProLinkPackets, TheMasterTakeoverPackets)
{
	const auto request = prolink::masterRequest (5, "StemDeck B");
	ASSERT_EQ (request.size(), 0x28u);
	EXPECT_EQ (request[0x0a], 0x26);
	EXPECT_EQ (request[0x1f], 0x01);
	EXPECT_EQ (request[0x20], 0x00);
	EXPECT_EQ (request[0x21], 5);
	EXPECT_EQ (be16 (request, 0x22), 0x04);
	EXPECT_EQ (be32 (request, 0x24), 5u);

	const auto response = prolink::masterResponse (6, "StemDeck A");
	ASSERT_EQ (response.size(), 0x2cu);
	EXPECT_EQ (response[0x0a], 0x27);
	EXPECT_EQ (response[0x21], 6);
	EXPECT_EQ (be16 (response, 0x22), 0x08);
	EXPECT_EQ (be32 (response, 0x24), 6u);
	EXPECT_EQ (be32 (response, 0x28), 1u);

	using Type = prolink::Control::Type;
	const auto r = prolink::parseControl (request.data(), request.size());
	ASSERT_TRUE (r);
	EXPECT_EQ (r->type, Type::masterRequest);
	EXPECT_EQ (r->device, 5);
	const auto a = prolink::parseControl (response.data(), response.size());
	ASSERT_TRUE (a);
	EXPECT_EQ (a->type, Type::masterResponse);
	EXPECT_EQ (a->device, 6);
	EXPECT_EQ (a->value, 1);

	const auto command = prolink::syncCommand (0x21, "DJM-V10", prolink::Control::becomeMaster);
	ASSERT_EQ (command.size(), 0x2cu);
	EXPECT_EQ (command[0x0a], 0x2a);
	EXPECT_EQ (be16 (command, 0x22), 0x08);
	EXPECT_EQ (command[0x2b], 0x01);
	const auto c = prolink::parseControl (command.data(), command.size());
	ASSERT_TRUE (c);
	EXPECT_EQ (c->type, Type::syncCommand);
	EXPECT_EQ (c->device, 0x21);
	EXPECT_EQ (c->value, prolink::Control::becomeMaster);

	const auto beat = prolink::beatPacket (6, "StemDeck", 120.0, 1.0, 1);
	EXPECT_FALSE (prolink::parseControl (beat.data(), beat.size()));
	EXPECT_FALSE (prolink::parseControl (request.data(), 0x26)) << "too short";
}

// D6, startup.html, "Startup in a channel-specific port": the mixer's
// intention (01, 2f bytes, its IP at 24), the player's request (02 with 01 at
// 0b, D 00, sent to the mixer) and the mixer's assignment (03 with 01 at 0b,
// 27 bytes, D at 24).
TEST (ProLinkPackets, TheMixerAssignmentPackets)
{
	prolink::Identity mixer { 0x21, "DJM-V10", { 2, 0, 0, 0, 0, 0x21 }, htonlTest (192, 168, 8, 33) };
	const auto intention = prolink::mixerIntention (mixer);
	ASSERT_EQ (intention.size(), 0x2fu);
	EXPECT_EQ (intention[0x0a], 0x01);
	EXPECT_EQ (intention[0x2e], 0x01);
	const auto i = prolink::parseAnnouncement (intention.data(), intention.size());
	ASSERT_TRUE (i);
	EXPECT_EQ (i->type, prolink::Announcement::Type::mixerIntention);
	EXPECT_EQ (i->ip, mixer.ip);

	const auto request = prolink::numberRequest (deck6(), 1);
	ASSERT_EQ (request.size(), 0x32u);
	EXPECT_EQ (request[0x0a], 0x02);
	EXPECT_EQ (request[0x0b], 0x01);
	EXPECT_EQ (std::string ((const char*) request.data() + 0x0c, 8), "StemDeck");
	EXPECT_EQ (request[0x21], 0x03) << "the CDJ-3000 claim structure";
	EXPECT_EQ (be16 (request, 0x22), 0x32);
	EXPECT_EQ (std::memcmp (request.data() + 0x24, &testIp, 4), 0);
	EXPECT_EQ (std::memcmp (request.data() + 0x28, deck6().mac.data(), 6), 0);
	EXPECT_EQ (request[0x2e], 0x00) << "no number of its own";
	EXPECT_EQ (request[0x2f], 1);
	EXPECT_EQ (request[0x30], 0x01);

	const auto assignment = prolink::numberAssignment (mixer, 2, 1);
	ASSERT_EQ (assignment.size(), 0x27u);
	EXPECT_EQ (assignment[0x0a], 0x03);
	EXPECT_EQ (assignment[0x0b], 0x01);
	EXPECT_EQ (assignment[0x24], 2);
	const auto a = prolink::parseAnnouncement (assignment.data(), assignment.size());
	ASSERT_TRUE (a);
	EXPECT_EQ (a->type, prolink::Announcement::Type::numberAssigned);
	EXPECT_EQ (a->device, 2);
}
