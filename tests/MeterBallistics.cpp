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
