#include <gtest/gtest.h>

#include "MeterBallistics.h"

#include <cmath>

// One meter behaviour across the system (decided 2026-10-07): the sources send
// raw peaks, every display applies the same ballistics -- attack immediate,
// release 20 dB/s, the highest peak held 1.5 s, then falling at 20 dB/s.
// Core's truth carries the numbers ("meters"); these are the defaults.

namespace
{
	constexpr float tick = 1.0f / 25.0f;

	float db (float gain) { return 20.0f * std::log10 (gain); }
	float gain (float decibels) { return std::pow (10.0f, decibels / 20.0f); }

	void feedFor (PeakMeter& meter, float peak, float seconds, float step = tick)
	{
		for (float t = step * 0.5f; t < seconds; t += step)
			meter.feed (peak, step);
	}
}

TEST (MeterBallistics, TheDefaultsAreTheSystemsNumbers)
{
	const MeterParameters defaults;
	EXPECT_FLOAT_EQ (defaults.attackMs, 0.0f);
	EXPECT_FLOAT_EQ (defaults.releaseDbPerSecond, 20.0f);
	EXPECT_FLOAT_EQ (defaults.peakHoldSeconds, 1.5f);
	EXPECT_FLOAT_EQ (PeakMeter::floorDb, -60.0f) << "the bottom of StemDeck's meter scale";
}

TEST (MeterBallistics, AttackJumpsToThePeak)
{
	PeakMeter meter;
	meter.feed (0.8f, tick);
	EXPECT_FLOAT_EQ (meter.level(), 0.8f);
	EXPECT_FLOAT_EQ (meter.hold(), 0.8f);
}

TEST (MeterBallistics, ReleaseFallsTwentyDbASecond)
{
	PeakMeter meter;
	meter.feed (1.0f, tick);
	feedFor (meter, 0.0f, 1.0f);
	EXPECT_NEAR (db (meter.level()), -20.0f, 0.01f);
}

TEST (MeterBallistics, ReleaseDoesNotDependOnTheFrameRate)
{
	PeakMeter at25, at60;
	at25.feed (1.0f, tick);
	at60.feed (1.0f, 1.0f / 60.0f);
	feedFor (at25, 0.0f, 1.0f, 1.0f / 25.0f);
	feedFor (at60, 0.0f, 1.0f, 1.0f / 60.0f);
	EXPECT_NEAR (db (at25.level()), -20.0f, 0.01f);
	EXPECT_NEAR (db (at60.level()), -20.0f, 0.01f);
}

TEST (MeterBallistics, TheBarFallsNoLowerThanThePeak)
{
	PeakMeter meter;
	meter.feed (1.0f, tick);
	feedFor (meter, gain (-6.0f), 1.0f);
	EXPECT_NEAR (db (meter.level()), -6.0f, 0.01f);
}

TEST (MeterBallistics, TheHoldStaysForItsTime)
{
	PeakMeter meter;
	meter.feed (1.0f, tick);
	feedFor (meter, 0.0f, 1.4f);
	EXPECT_NEAR (db (meter.hold()), 0.0f, 0.01f);
	EXPECT_LT (db (meter.level()), -27.0f) << "the bar has fallen meanwhile";
}

TEST (MeterBallistics, ThenTheHoldFallsTwentyDbASecond)
{
	PeakMeter meter;
	meter.feed (1.0f, tick);
	feedFor (meter, 0.0f, 2.0f);
	EXPECT_NEAR (db (meter.hold()), -10.0f, 0.1f);
}

TEST (MeterBallistics, AHigherPeakRestartsTheHold)
{
	PeakMeter meter;
	meter.feed (gain (-20.0f), tick);
	feedFor (meter, 0.0f, 1.0f);
	meter.feed (gain (-10.0f), tick);
	feedFor (meter, 0.0f, 1.4f);
	EXPECT_NEAR (db (meter.hold()), -10.0f, 0.01f);
}

TEST (MeterBallistics, ALowerPeakDoesNotRestartTheHold)
{
	PeakMeter meter;
	meter.feed (1.0f, tick);
	feedFor (meter, 0.0f, 1.0f);
	meter.feed (gain (-6.0f), tick);
	feedFor (meter, 0.0f, 0.96f);
	EXPECT_NEAR (db (meter.hold()), -10.0f, 0.1f) << "2 s after the first peak";
}

TEST (MeterBallistics, TheHoldNeverSitsBelowTheBar)
{
	PeakMeter meter;
	for (int i = 0; i < 200; ++i)
	{
		meter.feed (i % 37 == 0 ? gain (-3.0f) : (i % 5 == 0 ? gain (-30.0f) : 0.0f), tick);
		EXPECT_GE (meter.hold(), meter.level()) << "tick " << i;
	}
}

TEST (MeterBallistics, SilenceReachesZero)
{
	PeakMeter meter;
	meter.feed (1.0f, tick);
	feedFor (meter, 0.0f, 3.1f);
	EXPECT_EQ (meter.level(), 0.0f);
	feedFor (meter, 0.0f, 1.5f);
	EXPECT_EQ (meter.hold(), 0.0f);
}

TEST (MeterBallistics, APeakBelowTheFloorShowsNothing)
{
	PeakMeter meter;
	meter.feed (gain (-70.0f), tick);
	EXPECT_EQ (meter.level(), 0.0f);
	EXPECT_EQ (meter.hold(), 0.0f);
}

TEST (MeterBallistics, TheParametersComeFromOutside)
{
	PeakMeter meter ({ 0.0f, 50.0f, 0.4f });
	meter.feed (1.0f, tick);
	feedFor (meter, 0.0f, 0.4f);
	EXPECT_NEAR (db (meter.level()), -20.0f, 0.01f);
	EXPECT_NEAR (db (meter.hold()), 0.0f, 0.01f);
	feedFor (meter, 0.0f, 0.4f);
	EXPECT_NEAR (db (meter.hold()), -20.0f, 0.1f);
}

TEST (MeterBallistics, AnAttackTimeSlowsTheRise)
{
	PeakMeter meter ({ 100.0f, 20.0f, 1.5f });
	meter.feed (1.0f, 0.01f);
	EXPECT_LT (meter.level(), 0.9f) << "10 ms into a 100 ms attack";
	EXPECT_GT (meter.level(), 0.0f);
	feedFor (meter, 1.0f, 1.0f, 0.01f);
	EXPECT_NEAR (db (meter.level()), 0.0f, 0.1f) << "there after ten attack times";
}

// The bar and its hold line light the same segment for the same level.
TEST (MeterBallistics, SegmentsLitSpanTheScale)
{
	EXPECT_EQ (segmentsLit (0.0f, 23), 0);
	EXPECT_EQ (segmentsLit (1.0f, 23), 23);
	EXPECT_EQ (segmentsLit (2.0f, 23), 23) << "capped at full scale";
	EXPECT_EQ (segmentsLit (gain (-30.0f), 24), 12);
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
