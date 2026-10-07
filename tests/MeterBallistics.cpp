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
	EXPECT_FLOAT_EQ (meter.levelDb(), db (0.8f));
	EXPECT_FLOAT_EQ (meter.holdDb(), db (0.8f));
}

TEST (MeterBallistics, ReleaseFallsTwentyDbASecond)
{
	PeakMeter meter;
	meter.feed (1.0f, tick);
	feedFor (meter, 0.0f, 1.0f);
	EXPECT_NEAR (meter.levelDb(), -20.0f, 0.01f);
}

TEST (MeterBallistics, ReleaseDoesNotDependOnTheFrameRate)
{
	PeakMeter at25, at60;
	at25.feed (1.0f, tick);
	at60.feed (1.0f, 1.0f / 60.0f);
	feedFor (at25, 0.0f, 1.0f, 1.0f / 25.0f);
	feedFor (at60, 0.0f, 1.0f, 1.0f / 60.0f);
	EXPECT_NEAR (at25.levelDb(), -20.0f, 0.01f);
	EXPECT_NEAR (at60.levelDb(), -20.0f, 0.01f);
}

TEST (MeterBallistics, TheBarFallsNoLowerThanThePeak)
{
	PeakMeter meter;
	meter.feed (1.0f, tick);
	feedFor (meter, gain (-6.0f), 1.0f);
	EXPECT_NEAR (meter.levelDb(), -6.0f, 0.01f);
}

TEST (MeterBallistics, TheHoldStaysForItsTime)
{
	PeakMeter meter;
	meter.feed (1.0f, tick);
	feedFor (meter, 0.0f, 1.4f);
	EXPECT_NEAR (meter.holdDb(), 0.0f, 0.01f);
	EXPECT_LT (meter.levelDb(), -27.0f) << "the bar has fallen meanwhile";
}

TEST (MeterBallistics, ThenTheHoldFallsTwentyDbASecond)
{
	PeakMeter meter;
	meter.feed (1.0f, tick);
	feedFor (meter, 0.0f, 2.0f);
	EXPECT_NEAR (meter.holdDb(), -10.0f, 0.1f);
}

TEST (MeterBallistics, AHigherPeakRestartsTheHold)
{
	PeakMeter meter;
	meter.feed (gain (-20.0f), tick);
	feedFor (meter, 0.0f, 1.0f);
	meter.feed (gain (-10.0f), tick);
	feedFor (meter, 0.0f, 1.4f);
	EXPECT_NEAR (meter.holdDb(), -10.0f, 0.01f);
}

TEST (MeterBallistics, ALowerPeakDoesNotRestartTheHold)
{
	PeakMeter meter;
	meter.feed (1.0f, tick);
	feedFor (meter, 0.0f, 1.0f);
	meter.feed (gain (-6.0f), tick);
	feedFor (meter, 0.0f, 0.96f);
	EXPECT_NEAR (meter.holdDb(), -10.0f, 0.1f) << "2 s after the first peak";
}

TEST (MeterBallistics, TheHoldNeverSitsBelowTheBar)
{
	PeakMeter meter;
	for (int i = 0; i < 200; ++i)
	{
		meter.feed (i % 37 == 0 ? gain (-3.0f) : (i % 5 == 0 ? gain (-30.0f) : 0.0f), tick);
		EXPECT_GE (meter.holdDb(), meter.levelDb()) << "tick " << i;
	}
}

TEST (MeterBallistics, SilenceReachesZero)
{
	PeakMeter meter;
	meter.feed (1.0f, tick);
	feedFor (meter, 0.0f, 3.1f);
	EXPECT_EQ (meter.levelDb(), PeakMeter::floorDb);
	feedFor (meter, 0.0f, 1.5f);
	EXPECT_EQ (meter.holdDb(), PeakMeter::floorDb);
}

TEST (MeterBallistics, APeakBelowTheFloorShowsNothing)
{
	PeakMeter meter;
	meter.feed (gain (-70.0f), tick);
	EXPECT_EQ (meter.levelDb(), PeakMeter::floorDb);
	EXPECT_EQ (meter.holdDb(), PeakMeter::floorDb);
}

TEST (MeterBallistics, TheParametersComeFromOutside)
{
	PeakMeter meter ({ 0.0f, 50.0f, 0.4f });
	meter.feed (1.0f, tick);
	feedFor (meter, 0.0f, 0.4f);
	EXPECT_NEAR (meter.levelDb(), -20.0f, 0.01f);
	EXPECT_NEAR (meter.holdDb(), 0.0f, 0.01f);
	feedFor (meter, 0.0f, 0.4f);
	EXPECT_NEAR (meter.holdDb(), -20.0f, 0.1f);
}

TEST (MeterBallistics, AnAttackTimeSlowsTheRise)
{
	PeakMeter meter ({ 100.0f, 20.0f, 1.5f });
	meter.feed (1.0f, 0.01f);
	EXPECT_LT (meter.levelDb(), db (0.9f)) << "10 ms into a 100 ms attack";
	EXPECT_GT (meter.levelDb(), PeakMeter::floorDb);
	feedFor (meter, 1.0f, 1.0f, 0.01f);
	EXPECT_NEAR (meter.levelDb(), 0.0f, 0.1f) << "there after ten attack times";
}

// StemDeck's bars stand where the desk's do (decided 2026-10-07): the desk's
// LED scale (a3-mixer a3_mixer_meters.py), each threshold k at k/8 of the
// bar, linear in dB between them, below -36 on to an empty bar at -48.
TEST (MeterScale, EveryLedThresholdStandsAtItsEighth)
{
	const float thresholds[] { -36.0f, -24.0f, -18.0f, -12.0f, -9.0f, -6.0f, -3.0f, 0.0f };
	for (int k = 1; k <= 8; ++k)
		EXPECT_FLOAT_EQ (barFraction (thresholds[k - 1]), (float) k / 8.0f) << thresholds[k - 1] << " dB";
}

TEST (MeterScale, LinearInDbBetweenThresholds)
{
	EXPECT_FLOAT_EQ (barFraction (-30.0f), 1.5f / 8.0f);
	EXPECT_FLOAT_EQ (barFraction (-7.5f), 5.5f / 8.0f);
}

TEST (MeterScale, BelowTheFirstLedTheBarRunsOnToMinus48)
{
	EXPECT_FLOAT_EQ (barFraction (-42.0f), 0.5f / 8.0f);
	EXPECT_FLOAT_EQ (barFraction (-48.0f), 0.0f);
	EXPECT_FLOAT_EQ (barFraction (-60.0f), 0.0f);
	EXPECT_FLOAT_EQ (meterScaleFloorDb, -48.0f);
}

TEST (MeterScale, FullScaleAndOverFillTheBar)
{
	EXPECT_FLOAT_EQ (barFraction (0.0f), 1.0f);
	EXPECT_FLOAT_EQ (barFraction (6.0f), 1.0f);
}

// The bar and its hold line light the same segment for the same level: a
// segment lights once the level reaches its top, as an LED at its threshold.
TEST (MeterScale, SegmentsLitFollowTheScale)
{
	EXPECT_EQ (segmentsLit (PeakMeter::floorDb, 24), 0);
	EXPECT_EQ (segmentsLit (0.0f, 23), 23);
	EXPECT_EQ (segmentsLit (3.0f, 23), 23) << "capped at full scale";
	EXPECT_EQ (segmentsLit (-12.0f, 24), 12);
	EXPECT_EQ (segmentsLit (-12.1f, 24), 11);
	EXPECT_EQ (segmentsLit (-3.0f, 8), 7) << "eight segments are the desk's eight LEDs";
}

// The desk's LED colours: green up to -12, yellow -9 and -6, red -3 and 0.
TEST (MeterScale, SegmentColoursAreTheDesksLeds)
{
	EXPECT_EQ (zoneOfSegment (3, 8), MeterZone::green) << "the -12 LED";
	EXPECT_EQ (zoneOfSegment (4, 8), MeterZone::yellow) << "the -9 LED";
	EXPECT_EQ (zoneOfSegment (5, 8), MeterZone::yellow) << "the -6 LED";
	EXPECT_EQ (zoneOfSegment (6, 8), MeterZone::red) << "the -3 LED";
	EXPECT_EQ (zoneOfSegment (7, 8), MeterZone::red) << "the 0 LED";
	EXPECT_EQ (zoneOfSegment (11, 24), MeterZone::green);
	EXPECT_EQ (zoneOfSegment (12, 24), MeterZone::yellow);
	EXPECT_EQ (zoneOfSegment (17, 24), MeterZone::yellow);
	EXPECT_EQ (zoneOfSegment (18, 24), MeterZone::red);
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
