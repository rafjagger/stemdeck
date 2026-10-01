#include <gtest/gtest.h>

#include "Remote.h"

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
	EXPECT_EQ (remote::maskOf ({ true, false, false, false, true, false }), 0b010001u);
	EXPECT_EQ (remote::maskOf ({}), 0u);
}

// One meter per stem to the desk: rms over both channels of the stem,
// from the sum of squares the audio thread gathered since the last send.
TEST (Remote, RmsIsTheRootOfTheMeanSquare)
{
	EXPECT_FLOAT_EQ (remote::rmsOf (4 * 0.25, 4), 0.5f);
	EXPECT_FLOAT_EQ (remote::rmsOf (0.0, 0), 0.0f);    // nothing played: silence, not NaN
}
