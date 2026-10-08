#include <gtest/gtest.h>

#include "WaveformScale.h"

#include <cmath>
#include <limits>

namespace
{
	// Distance between two neighbouring beats on screen, in pixels.
	double beatSpacingPx (double nativeBpm, double rate, double widthPx = 800.0, double visibleSeconds = 8.0)
	{
		return (60.0 / nativeBpm) * waveformScale::pixelsPerTrackSecond (widthPx, visibleSeconds, rate);
	}
}

TEST (WaveformScale, TwoSyncedDecksShowTheSameBeatSpacing)
{
	const auto syncedBpm = 128.0;
	const auto slow = beatSpacingPx (120.0, syncedBpm / 120.0);
	const auto fast = beatSpacingPx (128.0, syncedBpm / 128.0);
	EXPECT_NEAR (slow, fast, 1e-9);
}

TEST (WaveformScale, RateOneKeepsTheTrackWindow)
{
	EXPECT_DOUBLE_EQ (waveformScale::trackSpan (8.0, 1.0), 8.0);
	EXPECT_DOUBLE_EQ (waveformScale::pixelsPerTrackSecond (800.0, 8.0, 1.0), 100.0);
}

TEST (WaveformScale, AFasterDeckShowsMoreTrack)
{
	EXPECT_DOUBLE_EQ (waveformScale::trackSpan (8.0, 1.25), 10.0);
}

TEST (WaveformScale, ARateThatMeansNothingFallsBackToOne)
{
	const auto nan = std::numeric_limits<double>::quiet_NaN();
	const auto inf = std::numeric_limits<double>::infinity();
	for (const auto rate : { 0.0, -1.0, nan, inf, -inf })
	{
		EXPECT_DOUBLE_EQ (waveformScale::sanitisedRate (rate), 1.0);
		EXPECT_DOUBLE_EQ (waveformScale::trackSpan (8.0, rate), 8.0);
		EXPECT_TRUE (std::isfinite (waveformScale::pixelsPerTrackSecond (800.0, 8.0, rate)));
	}
}
