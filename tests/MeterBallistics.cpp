#include <gtest/gtest.h>

#include "MeterBallistics.h"

namespace
{
	float releaseFor (float level, int ticks)
	{
		for (int i = 0; i < ticks; ++i)
			level = nextMeterLevel (level, 0.0f);

		return level;
	}
}

TEST (MeterBallistics, AttackJumpsToThePeak)
{
	EXPECT_FLOAT_EQ (nextMeterLevel (0.1f, 0.8f), 0.8f);
}

TEST (MeterBallistics, ReleaseFallsSlowly)
{
	const auto next = nextMeterLevel (1.0f, 0.0f);
	EXPECT_LT (next, 1.0f);
	EXPECT_GT (next, 0.5f);
}

TEST (MeterBallistics, SilenceReachesZero)
{
	// Two seconds at 60 Hz: long enough for any release to finish.
	EXPECT_EQ (releaseFor (1.0f, 120), 0.0f);
}

TEST (MeterBallistics, QuietSignalStillReachesZero)
{
	// Starts below the old 0.001 step threshold / 0.15 (~ -43.5 dB) and must not stick there.
	EXPECT_EQ (releaseFor (0.005f, 120), 0.0f);
}

TEST (MeterBallistics, ASmallMeterDrawsSegmentsItCanShow)
{
	EXPECT_EQ (meterSegments (24.0f), 8) << "the top bar's REC meters";
	EXPECT_EQ (meterSegments (300.0f), 24) << "the mixer's, capped";
	EXPECT_GE (meterSegments (4.0f), 1);
}

// StemDeck's output meters light a clip lamp above full scale and hold it for
// about a second, long enough to be seen after a single peak.
TEST (ClipHold, FullScaleIsNotAClip)
{
	ClipHold hold;
	EXPECT_FALSE (hold.feed (1.0f, 1.0f / 60.0f));
	EXPECT_FALSE (hold.feed (0.5f, 1.0f / 60.0f));
}

TEST (ClipHold, AboveFullScaleLightsAtOnce)
{
	ClipHold hold;
	EXPECT_TRUE (hold.feed (1.01f, 1.0f / 60.0f));
}

TEST (ClipHold, HoldsAboutASecondAfterThePeak)
{
	ClipHold hold;
	hold.feed (1.5f, 1.0f / 60.0f);

	for (int tick = 0; tick < 54; ++tick)   // 0.9 s at 60 Hz
		EXPECT_TRUE (hold.feed (0.0f, 1.0f / 60.0f)) << "tick " << tick;

	for (int tick = 0; tick < 12; ++tick)   // past 1.1 s
		hold.feed (0.0f, 1.0f / 60.0f);
	EXPECT_FALSE (hold.feed (0.0f, 1.0f / 60.0f));
}

TEST (ClipHold, ANewClipRestartsTheHold)
{
	ClipHold hold;
	hold.feed (2.0f, 0.8f);
	hold.feed (2.0f, 0.1f);
	EXPECT_TRUE (hold.feed (0.0f, 0.8f)) << "0.8 s after the second clip, not 1.6 s after the first";
}
