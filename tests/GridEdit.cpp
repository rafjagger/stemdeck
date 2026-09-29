#include <gtest/gtest.h>

#include "GridEdit.h"

#include <cmath>

using GridEdit::Grid;

namespace
{
	constexpr Grid at120 { 120.0, 0.25 };   // a beat is 0.5 s, a bar 2 s
}

TEST (GridEdit, FirstBeatStaysWithinTheFirstBar)
{
	EXPECT_NEAR (GridEdit::normalised ({ 120.0, 5.1 }).firstBeat, 1.1, 1e-9) << "two bars earlier, same downbeat";
	EXPECT_NEAR (GridEdit::normalised ({ 120.0, -0.5 }).firstBeat, 1.5, 1e-9);
	EXPECT_NEAR (GridEdit::normalised ({ 120.0, 0.0 }).firstBeat, 0.0, 1e-9);
}

TEST (GridEdit, TheJogShiftsTheWholeGrid)
{
	EXPECT_NEAR (GridEdit::shift (at120, 0.01).firstBeat, 0.26, 1e-9);
	EXPECT_NEAR (GridEdit::shift (at120, -0.3).firstBeat, 1.95, 1e-9) << "past zero: a bar on, same downbeat";
	EXPECT_DOUBLE_EQ (GridEdit::shift (at120, 0.01).bpm, 120.0);
}

TEST (GridEdit, HalfABeatEachWay)
{
	EXPECT_NEAR (GridEdit::shiftHalfBeat (at120, true).firstBeat, 0.5, 1e-9);
	EXPECT_NEAR (GridEdit::shiftHalfBeat (at120, false).firstBeat, 0.0, 1e-9);
	EXPECT_NEAR (GridEdit::shiftHalfBeat ({ 120.0, 0.0 }, false).firstBeat, 1.75, 1e-9) << "before zero: a bar on";
}

TEST (GridEdit, SnapPutsTheDownbeatOnTheCue)
{
	const auto snapped = GridEdit::snapToCue (at120, 33.75);
	EXPECT_NEAR (snapped.firstBeat, 1.75, 1e-9) << "33.75 is 16 bars after 1.75";
	const auto beatsAtCue = (33.75 - snapped.firstBeat) / 0.5;
	EXPECT_NEAR (std::fmod (beatsAtCue, 4.0), 0.0, 1e-9) << "the cue is a 1";
}

TEST (GridEdit, ShiftGridTakesTheLeadersPhase)
{
	// At 10.1 s this deck is 0.1 s past a beat of its grid; the leader is
	// exactly on one. The grid moves 0.1 s later.
	const Grid grid { 120.0, 0.0 };
	const auto shifted = GridEdit::shiftToPhase (grid, 10.1, 7.0);   // own phase .2, leader .0
	const auto phase = (10.1 - shifted.firstBeat) / 0.5;
	EXPECT_NEAR (phase - std::round (phase), 0.0, 1e-9) << "now on a beat, like the leader";
	EXPECT_NEAR (std::abs (shifted.firstBeat - 0.1), 0.0, 1e-9) << "the short way: 0.1 s, not 0.4";
}

TEST (GridEdit, NoTempoNoEdit)
{
	const Grid none {};
	EXPECT_DOUBLE_EQ (GridEdit::shiftHalfBeat (none, true).firstBeat, 0.0);
	EXPECT_DOUBLE_EQ (GridEdit::shiftToPhase (none, 3.0, 1.2).firstBeat, 0.0);
}

// "SET 1": the bar's one wherever the playhead is -- between two beats of the
// old grid, which a cue point rarely is. The same arithmetic as SNAP.
TEST (GridEdit, TheDownbeatGoesToThePlayhead)
{
	const auto playhead = 12.34;
	const auto set = GridEdit::snapToCue (at120, playhead);
	EXPECT_NEAR (std::fmod ((playhead - set.firstBeat) / 0.5, 4.0), 0.0, 1e-9) << "the playhead is a 1";
	EXPECT_GE (set.firstBeat, 0.0);
	EXPECT_LT (set.firstBeat, 2.0) << "within the first bar";
	EXPECT_DOUBLE_EQ (set.bpm, 120.0);
}

// Loop in/out on the nearest beats of the grid (a beat is 0.5 s at 120, the
// first at 0.25).
TEST (GridEdit, ALoopSnapsToTheNearestBeats)
{
	const auto loop = GridEdit::snappedLoop (at120, 10.2, 12.1);
	EXPECT_NEAR (loop.start, 10.25, 1e-9);
	EXPECT_NEAR (loop.end, 12.25, 1e-9);
}

TEST (GridEdit, ALoopInsideOneBeatBecomesOneBeat)
{
	const auto loop = GridEdit::snappedLoop (at120, 10.2, 10.3);
	EXPECT_NEAR (loop.start, 10.25, 1e-9);
	EXPECT_NEAR (loop.end, 10.75, 1e-9);
}

TEST (GridEdit, WithoutATempoTheLoopStaysPut)
{
	const auto loop = GridEdit::snappedLoop ({ 0.0, 0.0 }, 10.2, 12.1);
	EXPECT_DOUBLE_EQ (loop.start, 10.2);
	EXPECT_DOUBLE_EQ (loop.end, 12.1);
}
