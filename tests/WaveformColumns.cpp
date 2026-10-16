#include <gtest/gtest.h>

#include "WaveformColumns.h"

#include <cmath>

namespace
{
	constexpr double span = 8.0;   // seconds of track in the window
	constexpr int width = 768;     // the rig's waveform, in pixels
	constexpr double playheadX = width / 2.0;

	waveformColumns::Grid gridAt (double position)
	{
		return waveformColumns::gridFor (position, span, width, playheadX);
	}
}

// The shimmer (2026-10-16): each frame resampled the track from a fractional
// start time, so a peak's column held other samples every frame. A column
// must cover the same stretch of track wherever the view has scrolled to.
TEST (WaveformColumns, TheColumnHoldingAMomentCoversTheSameTrackAtEveryScrollPosition)
{
	const double moment = 61.2345;
	const auto first = gridAt (60.0);

	for (const auto position : { 60.0013, 60.0071, 60.0166, 60.1, 61.3337 })
	{
		const auto later = gridAt (position);
		const auto k = waveformColumns::columnAt (moment, later);
		EXPECT_EQ (k, waveformColumns::columnAt (moment, first));
		EXPECT_EQ (waveformColumns::columnStart (k, later), waveformColumns::columnStart (k, first));
		EXPECT_EQ (waveformColumns::columnEnd (k, later), waveformColumns::columnEnd (k, first));
	}
}

// The boundary of a column is a fixed moment of the track; from one frame to
// the next it may move only by whole pixels, and a column is drawn at its
// boundary.
TEST (WaveformColumns, TheViewScrollsByWholePixels)
{
	const auto first = gridAt (60.0);
	const auto k = waveformColumns::columnAt (61.2345, first);
	const auto boundary = waveformColumns::columnStart (k, first);
	const auto x0 = waveformColumns::timeToX (boundary, first);

	for (const auto position : { 60.0013, 60.0071, 60.0166, 60.1 })
	{
		const auto later = gridAt (position);
		const auto x = waveformColumns::timeToX (boundary, later);
		EXPECT_NEAR (x0 - x, std::round (x0 - x), 1e-6) << "at " << position;
		EXPECT_NEAR (x, waveformColumns::columnX (k, later), 1e-6) << "at " << position;
	}
}

TEST (WaveformColumns, ColumnsTileTheTrackWithoutGapsOrOverlap)
{
	const auto grid = gridAt (30.0);
	for (long long k = grid.originColumn; k < grid.originColumn + width; ++k)
		EXPECT_EQ (waveformColumns::columnEnd (k, grid), waveformColumns::columnStart (k + 1, grid));
}

TEST (WaveformColumns, ThePlayheadStaysWithinHalfAPixelOfItsPlace)
{
	for (const auto position : { 0.0, 0.004, 12.3456, 59.99999, 200.5 })
	{
		const auto grid = gridAt (position);
		EXPECT_NEAR (waveformColumns::timeToX (position, grid), playheadX, 0.5) << "at " << position;
	}
}

TEST (WaveformColumns, OverlaysUseTheColumnsScale)
{
	const auto grid = gridAt (42.0);
	const auto k = grid.originColumn + 100;
	EXPECT_NEAR (waveformColumns::timeToX (waveformColumns::columnStart (k, grid), grid), 100.0, 1e-6);
}

// A sync nudge or a fader step far below a pixel must not move the column
// boundaries, or every column of the view would be resampled at once.
TEST (WaveformColumns, AHairlineChangeOfSpanKeepsTheColumnWidth)
{
	const auto a = waveformColumns::secondsPerColumn (span, width);
	const auto b = waveformColumns::secondsPerColumn (span * 1.00002, width);
	EXPECT_EQ (a, b);
	EXPECT_NEAR (a, span / width, 1.0 / waveformColumns::quantaPerSecond);
}

TEST (WaveformColumns, ANonsenseSpanStillGivesAColumn)
{
	EXPECT_GT (waveformColumns::secondsPerColumn (0.0, width), 0.0);
	EXPECT_GT (waveformColumns::secondsPerColumn (span, 0), 0.0);
}
