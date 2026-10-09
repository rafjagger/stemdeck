#include <gtest/gtest.h>

#include "KeyLock.h"

#include <cmath>

using keylock::PositionMap;

TEST (KeyLock, TheTimeRatioIsTheInverseOfTheRate)
{
	EXPECT_DOUBLE_EQ (keylock::timeRatio (1.0), 1.0);
	EXPECT_DOUBLE_EQ (keylock::timeRatio (1.25), 0.8);
	EXPECT_NEAR (keylock::timeRatio (0.92), 1.0 / 0.92, 1e-12);
}

// A held jog or a fader at an end must not hand the stretcher a ratio it
// cannot take (0 or infinite).
TEST (KeyLock, TheTimeRatioStaysFiniteAtAStandstill)
{
	EXPECT_TRUE (std::isfinite (keylock::timeRatio (0.0)));
	EXPECT_GT (keylock::timeRatio (0.0), 1.0);
	EXPECT_LT (keylock::timeRatio (100.0), 1.0);
	EXPECT_GT (keylock::timeRatio (100.0), 0.0);
}

// Off: the resampler plays the tempo, so the pitch follows it. On: Rubber
// Band plays the tempo; the resampler only converts the file's rate to the
// device's.
TEST (KeyLock, TheResamplerOnlyConvertsTheRateWhileKeyLocked)
{
	EXPECT_DOUBLE_EQ (keylock::resamplingRatio (1.08, 44100.0, 48000.0, false), 1.08 * 44100.0 / 48000.0);
	EXPECT_DOUBLE_EQ (keylock::resamplingRatio (1.08, 44100.0, 48000.0, true), 44100.0 / 48000.0);
	EXPECT_DOUBLE_EQ (keylock::resamplingRatio (0.9, 48000.0, 48000.0, true), 1.0);
}

// A scratch is pitch by nature: the record under the hand plays as it moves.
TEST (KeyLock, ScratchingBypassesTheStretcher)
{
	EXPECT_TRUE (keylock::usesStretcher (true, true, false));
	EXPECT_FALSE (keylock::usesStretcher (true, true, true));
	EXPECT_FALSE (keylock::usesStretcher (false, true, false));
	EXPECT_FALSE (keylock::usesStretcher (true, false, false)) << "no stretcher built yet: the plain path";
}

//==============================================================================
TEST (KeyLockPosition, AfterARestartThePlayheadIsWhereItStarted)
{
	PositionMap map;
	map.restart (1000);
	EXPECT_DOUBLE_EQ (map.position(), 1000.0);
	EXPECT_DOUBLE_EQ (map.latency(), 0.0);
}

// What the stretcher took in runs ahead of what is heard: the playhead is
// what has come out, in input samples, not where the reader is.
TEST (KeyLockPosition, ThePlayheadIsWhatWasHeardNotWhatWasRead)
{
	PositionMap map;
	map.restart (1000);
	map.fed (4000);
	map.heard (512 * 1.25);
	EXPECT_DOUBLE_EQ (map.position(), 1000.0 + 640.0);
	EXPECT_DOUBLE_EQ (map.latency(), 4000.0 - 640.0);
}

// The reader wraps at the loop's end long before the wrap is heard: the
// playhead jumps back only when the output gets there.
TEST (KeyLockPosition, ALoopJumpCountsWhenItIsHeard)
{
	PositionMap map;
	map.restart (1000);   // loop 2000 .. 4000
	map.fed (3000);       // the reader is at 4000, the loop's end
	map.jumped (2000);
	map.fed (1000);
	map.heard (2500);
	EXPECT_DOUBLE_EQ (map.position(), 3500.0) << "not jumped yet";
	map.heard (600);
	EXPECT_DOUBLE_EQ (map.position(), 2100.0);
}

// A loop shorter than the stretcher's latency: several jumps in flight.
TEST (KeyLockPosition, ManyShortLoopsInFlight)
{
	PositionMap map;
	map.restart (0);   // loop 0 .. 441
	for (int i = 0; i < 200; ++i)
	{
		map.fed (441);
		map.jumped (0);
	}
	map.heard (441.0 * 199 + 100.0);
	EXPECT_DOUBLE_EQ (map.position(), 100.0);
	map.heard (400.0);
	EXPECT_DOUBLE_EQ (map.position(), 59.0);
}

TEST (KeyLockPosition, ARestartForgetsTheJumps)
{
	PositionMap map;
	map.restart (0);
	map.fed (100);
	map.jumped (5000);
	map.restart (300);
	map.fed (50);
	map.heard (120);
	EXPECT_DOUBLE_EQ (map.position(), 420.0);
}

//==============================================================================
// The stretcher makes its output when it is fed, at the ratio of that
// moment, and hands it out later. What comes out stands for the input at
// the rate it was made with, not the rate when it is taken: a tempo change
// reaches the ear one latency later.
TEST (KeyLockOutputRuns, OutputStandsForTheRateItWasMadeAt)
{
	keylock::OutputRuns runs;
	runs.produced (100, 1.0);
	runs.produced (100, 1.25);
	EXPECT_DOUBLE_EQ (runs.consume (150, 2.0), 100.0 + 50 * 1.25);
	EXPECT_DOUBLE_EQ (runs.consume (50, 2.0), 50 * 1.25);
}

TEST (KeyLockOutputRuns, UnaccountedOutputTakesTheCurrentRate)
{
	keylock::OutputRuns runs;
	runs.produced (10, 1.0);
	EXPECT_DOUBLE_EQ (runs.consume (30, 0.5), 10.0 + 20 * 0.5);
}

// A sync nudge changes the rate every block: more runs than the record
// holds are merged, never lost.
TEST (KeyLockOutputRuns, ManyRunsAreMergedNotLost)
{
	keylock::OutputRuns runs;
	double expected = 0.0;
	for (int i = 0; i < 1000; ++i)
	{
		const auto rate = 1.0 + (i % 7) * 0.001;
		runs.produced (64, rate);
		expected += 64 * rate;
	}
	EXPECT_NEAR (runs.consume (64000, 1.0), expected, 1e-6);
}

TEST (KeyLockOutputRuns, ClearForgetsEverything)
{
	keylock::OutputRuns runs;
	runs.produced (100, 2.0);
	runs.clear();
	EXPECT_DOUBLE_EQ (runs.consume (10, 1.0), 10.0);
}

//==============================================================================
// Rubber Band lines its output up with its input exactly only at rate 1:
// away from it the first output sample lies a little before or after the
// start (startOffset, in input samples per unit of rate away from 1), and a
// tempo change moves what is heard against what was counted (rateLag).
// Both are measured on the stretcher itself; these are the corrections.
TEST (KeyLockAlignment, AtRateOneNothingIsCorrected)
{
	const keylock::Alignment alignment { 400.0, 1400.0 };
	EXPECT_EQ (keylock::startDiscard (1024, 1.0, alignment), 1024);
	EXPECT_DOUBLE_EQ (keylock::lagCorrection (alignment, 1.0, 1.0), 0.0);
	EXPECT_DOUBLE_EQ (keylock::lagCorrection (alignment, 1.08, 1.08), 0.0) << "no change since the start";
}

// Faster, the stretcher's output starts behind the start: more of it is
// thrown away; slower, less.
TEST (KeyLockAlignment, TheStartDiscardFollowsTheRate)
{
	const keylock::Alignment alignment { 400.0, 0.0 };
	EXPECT_EQ (keylock::startDiscard (1024, 1.25, alignment), 1024 + 80);    // 100 input samples at 1.25
	EXPECT_EQ (keylock::startDiscard (1024, 0.8, alignment), 1024 - 100);    // -80 input samples at 0.8
	EXPECT_GE (keylock::startDiscard (10, 0.05, alignment), 0) << "never less than nothing";
}

TEST (KeyLockAlignment, ATempoChangeSinceTheStartIsCorrected)
{
	const keylock::Alignment alignment { 0.0, 1400.0 };
	EXPECT_NEAR (keylock::lagCorrection (alignment, 1.08, 1.0), 1400.0 * 0.08, 1e-9);
	EXPECT_NEAR (keylock::lagCorrection (alignment, 1.0, 1.08), -1400.0 * 0.08, 1e-9);
}
