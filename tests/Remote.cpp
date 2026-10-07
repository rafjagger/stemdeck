#include <gtest/gtest.h>

#include "Remote.h"

#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

// The desk switches StemDeck's buses through Core (spec stemdeck-remote).
// The patterns are the truth's; here they are written out as a3-osc.json
// has them, the way tests/OscTruth.cpp writes out its listeners.

namespace
{
	const remote::Words words { "/stemdeck/{deck}/{stem}/bus/{bus}", "/stemdeck/{deck}/{stem}/buses",
								"/stemdeck/recall", "/vu/{n}", "/device/hello" };
}

TEST (Remote, ASwitchCommandIsParsed)
{
	const auto s = remote::parseSwitch (words, "/stemdeck/2/3/bus/5", 1);
	ASSERT_TRUE (s.has_value());
	EXPECT_EQ (s->deck, 1);
	EXPECT_EQ (s->stem, 2);
	EXPECT_EQ (s->bus, 4);
	EXPECT_TRUE (s->on);
}

TEST (Remote, OffIsZero)
{
	const auto s = remote::parseSwitch (words, "/stemdeck/1/1/bus/1", 0);
	ASSERT_TRUE (s.has_value());
	EXPECT_FALSE (s->on);
}

TEST (Remote, AnythingOutOfRangeIsRefused)
{
	for (const char* a : { "/stemdeck/3/1/bus/1", "/stemdeck/1/5/bus/1", "/stemdeck/1/1/bus/7",
						   "/stemdeck/0/1/bus/1", "/stemdeck/1/1/bus/x", "/stemdeck/1/1/buses",
						   "/stemdeck/1/1/bus/1/x", "/stemdeck/1/1/bus/" })
		EXPECT_FALSE (remote::parseSwitch (words, a, 1).has_value()) << a;
	EXPECT_FALSE (remote::parseSwitch (words, "/stemdeck/1/1/bus/1", 2).has_value());
	EXPECT_FALSE (remote::parseSwitch (words, "/stemdeck/1/1/bus/1", -1).has_value());
}

TEST (Remote, AReportNamesTheStem)
{
	EXPECT_EQ (remote::reportAddress (words, 1, 3), "/stemdeck/2/4/buses");
}

TEST (Remote, TheRecallIsKnown)
{
	EXPECT_TRUE (remote::isRecall (words, "/stemdeck/recall"));
	EXPECT_FALSE (remote::isRecall (words, "/stemdeck/recall/x"));
}

TEST (Remote, OneMeterPerStemFromFortyOne)
{
	EXPECT_EQ (remote::vuAddress (words, 0, 0), "/vu/41");
	EXPECT_EQ (remote::vuAddress (words, 1, 3), "/vu/48");
}

TEST (Remote, AMaskHasOneBitPerBus)
{
	EXPECT_EQ (remote::maskOf ({ true, false, false, false, true }), 0b10001u);
	EXPECT_EQ (remote::maskOf ({}), 0u);
}

// StemDeck has no cue of its own since 2026-10-07: the cue is the desk
// channel after its whole chain, in REAPER. Bus 6 was StemDeck's CUE; a Core
// that still sends it (an older truth) switches nothing.
TEST (Remote, TheOldCueBusIsNoLongerASwitch)
{
	EXPECT_FALSE (remote::parseSwitch (words, "/stemdeck/1/1/bus/6", 1).has_value());
	EXPECT_FALSE (remote::parseSwitch (words, "/stemdeck/2/4/bus/6", 0).has_value());
	EXPECT_TRUE (remote::parseSwitch (words, "/stemdeck/2/4/bus/5", 1).has_value()) << "AUX stays";
}

// One meter per stem to the desk: rms over both channels of the stem,
// from the sum of squares the audio thread gathered since the last send.
TEST (Remote, RmsIsTheRootOfTheMeanSquare)
{
	EXPECT_FLOAT_EQ (remote::rmsOf (4 * 0.25, 4), 0.5f);
	EXPECT_FLOAT_EQ (remote::rmsOf (0.0, 0), 0.0f);    // nothing played: silence, not NaN
}

// StemDeck's AUX bus as a stereo pair on the desk (decided 2026-10-04): the
// numbers are the truth's, found by name in its vu_meters (/vu/n = index + 1).
namespace
{
	std::vector<std::string> theTruthsMeters()
	{
		std::vector<std::string> meters (40, "reaper_out");
		for (const char* stem : { "stem_a1", "stem_a2", "stem_a3", "stem_a4",
								  "stem_b1", "stem_b2", "stem_b3", "stem_b4" })
			meters.push_back (stem);
		meters.push_back ("stem_aux_L");
		meters.push_back ("stem_aux_R");
		return meters;
	}
}

TEST (Remote, AMetersNumberIsItsPlaceInTheList)
{
	const auto meters = theTruthsMeters();
	EXPECT_EQ (remote::meterNumber (meters, "stem_aux_L"), std::optional<int> (49));
	EXPECT_EQ (remote::meterNumber (meters, "stem_aux_R"), std::optional<int> (50));
	EXPECT_EQ (remote::meterNumber (meters, "stem_a1"), std::optional<int> (41));
}

TEST (Remote, AnUnknownMeterHasNoNumber)
{
	EXPECT_FALSE (remote::meterNumber (theTruthsMeters(), "stem_aux_C").has_value());
	EXPECT_FALSE (remote::meterNumber ({}, "stem_aux_L").has_value());
}

TEST (Remote, ANamedMeterHasItsAddress)
{
	auto withMeters = words;
	withMeters.meters = theTruthsMeters();
	EXPECT_EQ (remote::vuAddressNamed (withMeters, "stem_aux_R"), std::optional<std::string> ("/vu/50"));
	EXPECT_FALSE (remote::vuAddressNamed (words, "stem_aux_R").has_value());   // an older truth: no meter
}

TEST (Remote, ABlockGivesItsPeakAndSquares)
{
	const float samples[] { 0.5f, -0.75f, 0.25f, 0.0f };
	const auto block = remote::measure (samples, 4);
	EXPECT_FLOAT_EQ (block.peak, 0.75f);
	EXPECT_DOUBLE_EQ (block.squares, 0.25 + 0.5625 + 0.0625);
	EXPECT_EQ (block.samples, 4);
}

TEST (Remote, TheSidesOfAStereoBlockAreMeasuredApart)
{
	const float left[] { 1.0f, -1.0f }, right[] { 0.0f, 0.0f };
	remote::LevelTap tapL, tapR;
	tapL.add (remote::measure (left, 2));
	tapR.add (remote::measure (right, 2));
	const auto l = tapL.pop(), r = tapR.pop();
	EXPECT_FLOAT_EQ (l.peak, 1.0f);
	EXPECT_FLOAT_EQ (l.rms, 1.0f);
	EXPECT_FLOAT_EQ (r.peak, 0.0f);
	EXPECT_FLOAT_EQ (r.rms, 0.0f);
}

TEST (Remote, ATapGathersBlocksUntilItIsEmptied)
{
	const float loud[] { 0.5f, 0.5f }, quiet[] { 0.0f, 0.0f };
	remote::LevelTap tap;
	tap.add (remote::measure (loud, 2));
	tap.add (remote::measure (quiet, 2));
	const auto level = tap.pop();
	EXPECT_FLOAT_EQ (level.peak, 0.5f);
	EXPECT_FLOAT_EQ (level.rms, std::sqrt (0.5f * 0.5f / 2.0f));   // rms over all four samples
	const auto after = tap.pop();
	EXPECT_FLOAT_EQ (after.peak, 0.0f);
	EXPECT_FLOAT_EQ (after.rms, 0.0f);
}

// One tick's meters go to the desk as one bundle (rafjagger/stemdeck#6): the
// desk pays per datagram, and ten a tick from each StemDeck flooded it.
namespace
{
	std::array<remote::Level, remote::stemMeters> stemLevels()
	{
		std::array<remote::Level, remote::stemMeters> levels {};
		for (size_t i = 0; i < levels.size(); ++i)
			levels[i] = { 0.1f * (float) (i + 1), 0.01f * (float) (i + 1) };
		return levels;
	}
}

TEST (Remote, ATicksBundleHoldsEveryStemThenTheAuxPair)
{
	auto withMeters = words;
	withMeters.meters = theTruthsMeters();
	const auto bundle = remote::levelBundle (withMeters, stemLevels(), { remote::Level { 0.7f, 0.3f }, remote::Level { 0.6f, 0.2f } });

	ASSERT_EQ (bundle.size(), 10u);
	for (int n = 0; n < remote::stemMeters; ++n)
	{
		EXPECT_EQ (bundle[(size_t) n].address, "/vu/" + std::to_string (41 + n));
		EXPECT_FLOAT_EQ (bundle[(size_t) n].level.peak, 0.1f * (float) (n + 1));
		EXPECT_FLOAT_EQ (bundle[(size_t) n].level.rms, 0.01f * (float) (n + 1));
	}
	EXPECT_EQ (bundle[8].address, "/vu/49");
	EXPECT_FLOAT_EQ (bundle[8].level.peak, 0.7f);
	EXPECT_FLOAT_EQ (bundle[8].level.rms, 0.3f);
	EXPECT_EQ (bundle[9].address, "/vu/50");
	EXPECT_FLOAT_EQ (bundle[9].level.peak, 0.6f);
	EXPECT_FLOAT_EQ (bundle[9].level.rms, 0.2f);
}

TEST (Remote, AnOlderTruthsBundleHasNoAuxPair)
{
	const auto bundle = remote::levelBundle (words, stemLevels(), {});
	ASSERT_EQ (bundle.size(), 8u);
	EXPECT_EQ (bundle.front().address, "/vu/41");
	EXPECT_EQ (bundle.back().address, "/vu/48");
}

// Two StemDecks write the same /vu/41-50 (rafjagger/stemdeck#6): a silent one
// sends nothing, so it does not overwrite the playing one's levels with zeros
// -- except one zero bundle as it falls silent, so the desk's bars drop once.
namespace
{
	std::vector<remote::Level> quiet() { return std::vector<remote::Level> (10); }

	std::vector<remote::Level> playing()
	{
		auto levels = quiet();
		levels[3] = { 0.5f, 0.2f };
		return levels;
	}
}

TEST (Remote, SilentFromTheStartSendsNothing)
{
	remote::MeterGate gate;
	for (int tick = 0; tick < 5; ++tick)
		EXPECT_EQ (gate.next (quiet()), remote::MeterGate::skip);
}

TEST (Remote, SoundIsSentEveryTick)
{
	remote::MeterGate gate;
	for (int tick = 0; tick < 5; ++tick)
		EXPECT_EQ (gate.next (playing()), remote::MeterGate::send);
}

TEST (Remote, FallingSilentSendsOneZeroBundleThenNothing)
{
	remote::MeterGate gate;
	gate.next (playing());
	EXPECT_EQ (gate.next (quiet()), remote::MeterGate::sendZeros);
	EXPECT_EQ (gate.next (quiet()), remote::MeterGate::skip);
	EXPECT_EQ (gate.next (quiet()), remote::MeterGate::skip);
}

TEST (Remote, SoundAfterSilenceIsSentAgain)
{
	remote::MeterGate gate;
	gate.next (playing());
	gate.next (quiet());
	gate.next (quiet());
	EXPECT_EQ (gate.next (playing()), remote::MeterGate::send);
}

TEST (Remote, BelowMinusNinetyDbIsSilence)
{
	auto hiss = quiet();
	hiss[0] = { 1.0e-5f, 1.0e-5f };    // -100 dBFS
	remote::MeterGate gate;
	EXPECT_EQ (gate.next (hiss), remote::MeterGate::skip);
	hiss[0] = { 1.0e-3f, 1.0e-4f };    // -60 dBFS peak
	EXPECT_EQ (gate.next (hiss), remote::MeterGate::send);
}
