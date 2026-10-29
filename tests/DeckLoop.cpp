// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "DeckLoop.h"
#include "FakeTransportDeck.h"

// LOOP IN, LOOP OUT and LOOP as on a CDJ-3000 (manual, "Looping"): IN marks
// the start at the playhead, OUT the end and the loop plays at once; LOOP
// (RELOOP/EXIT) leaves the loop playing on, and a second press goes back into
// it from its start.
namespace
{
struct DeckLoopTest : ::testing::Test
{
	FakeTransportDeck deck;
	DeckLoop loop;

	void loopFrom (double in, double out)
	{
		deck.at = in;
		loop.in (deck);
		deck.at = out;
		loop.out (deck);
	}
};
}

TEST_F (DeckLoopTest, InMarksOutLoops)
{
	deck.playing = true;
	deck.at = 10.0;
	loop.in (deck);
	EXPECT_TRUE (loop.isInMarked());
	EXPECT_FALSE (deck.looping) << "IN alone sets no loop";

	deck.at = 14.0;
	loop.out (deck);
	EXPECT_TRUE (deck.looping);
	EXPECT_DOUBLE_EQ (deck.in, 10.0);
	EXPECT_DOUBLE_EQ (deck.out, 14.0);
	EXPECT_DOUBLE_EQ (deck.at, 10.0) << "the loop plays from its start";
	EXPECT_TRUE (deck.playing);
	EXPECT_TRUE (loop.isInMarked()) << "the mark stays: OUT closes from it again";
}

// "Quantized if the deck quantizes": the deck's grid places both ends.
TEST_F (DeckLoopTest, TheDecksGridPlacesTheEnds)
{
	deck.beat = 0.5;
	loopFrom (10.1, 13.8);
	EXPECT_DOUBLE_EQ (deck.in, 10.0);
	EXPECT_DOUBLE_EQ (deck.out, 14.0);
}

TEST_F (DeckLoopTest, OutWithoutInDoesNothing)
{
	deck.at = 5.0;
	loop.out (deck);
	EXPECT_FALSE (deck.looping);
}

TEST_F (DeckLoopTest, OutBeforeInDoesNothing)
{
	deck.at = 8.0;
	loop.in (deck);
	deck.at = 6.0;   // the DJ searched back
	loop.out (deck);
	EXPECT_FALSE (deck.looping);
	EXPECT_TRUE (loop.isInMarked()) << "the in point waits for an OUT after it";
}

TEST_F (DeckLoopTest, LoopExitsAndReloops)
{
	deck.playing = true;
	loopFrom (10.0, 14.0);
	deck.at = 12.0;

	loop.toggle (deck);
	EXPECT_FALSE (deck.looping) << "EXIT";
	EXPECT_TRUE (deck.playing) << "playing on";
	EXPECT_DOUBLE_EQ (deck.at, 12.0) << "from where it was";
	EXPECT_TRUE (loop.canReloop (deck));

	deck.at = 20.0;
	loop.toggle (deck);
	EXPECT_TRUE (deck.looping) << "RELOOP";
	EXPECT_DOUBLE_EQ (deck.in, 10.0);
	EXPECT_DOUBLE_EQ (deck.out, 14.0);
	EXPECT_DOUBLE_EQ (deck.at, 10.0) << "from the loop's start";
}

TEST_F (DeckLoopTest, LoopWithNothingToGoBackToDoesNothing)
{
	EXPECT_FALSE (loop.canReloop (deck));
	loop.toggle (deck);
	EXPECT_FALSE (deck.looping);
}

// A loop from elsewhere (the overview drag, a restored session) is left and
// gone back into the same way.
TEST_F (DeckLoopTest, ALoopSetElsewhereIsExitedAndReloopedToo)
{
	deck.setLoop (3.0, 5.0);
	EXPECT_TRUE (loop.canReloop (deck));
	loop.toggle (deck);
	EXPECT_FALSE (deck.looping);
	loop.toggle (deck);
	EXPECT_TRUE (deck.looping);
	EXPECT_DOUBLE_EQ (deck.in, 3.0);
	EXPECT_DOUBLE_EQ (deck.out, 5.0);
}

// A new IN and OUT while looping make the new loop.
TEST_F (DeckLoopTest, InAndOutWhileLoopingMakeANewLoop)
{
	loopFrom (10.0, 14.0);
	loopFrom (11.0, 12.0);
	EXPECT_DOUBLE_EQ (deck.in, 11.0);
	EXPECT_DOUBLE_EQ (deck.out, 12.0);
}

TEST_F (DeckLoopTest, NothingLoadedNothingMarked)
{
	deck.loaded = false;
	deck.at = 3.0;
	loop.in (deck);
	EXPECT_FALSE (loop.isInMarked());
}

TEST_F (DeckLoopTest, ResetForgetsInAndTheLoopToGoBackTo)
{
	loopFrom (10.0, 14.0);
	loop.toggle (deck);
	deck.at = 1.0;
	loop.in (deck);
	loop.reset();
	EXPECT_FALSE (loop.isInMarked());
	EXPECT_FALSE (loop.canReloop (deck));
}

// The SCS.3d's VINYL key is IN and OUT in one: the first press marks, the
// second (after it) loops; one before the mark marks again.
TEST_F (DeckLoopTest, OneKeyInThenOut)
{
	deck.at = 10.0;
	loop.inOrOut (deck);
	EXPECT_TRUE (loop.isInMarked());
	deck.at = 9.0;
	loop.inOrOut (deck);
	EXPECT_TRUE (loop.isInMarked());
	EXPECT_FALSE (deck.looping);
	deck.at = 13.0;
	loop.inOrOut (deck);
	EXPECT_TRUE (deck.looping);
	EXPECT_DOUBLE_EQ (deck.in, 9.0);
	EXPECT_DOUBLE_EQ (deck.out, 13.0);
}

// OUT is a toggle (the maintainer, 2026-10-28): "wenn an ausmachen, läuft
// weiter, loop out nochmal drücken schließt den loop an neuer stelle".
TEST_F (DeckLoopTest, OutWhileLoopingLeavesTheLoopPlayingOn)
{
	deck.playing = true;
	loopFrom (10.0, 14.0);
	deck.at = 12.5;

	loop.out (deck);
	EXPECT_FALSE (deck.looping);
	EXPECT_TRUE (deck.playing) << "playing on";
	EXPECT_DOUBLE_EQ (deck.at, 12.5) << "from where it was";
}

TEST_F (DeckLoopTest, OutAgainClosesTheLoopFromTheSameInAtTheNewPlayhead)
{
	deck.playing = true;
	loopFrom (10.0, 14.0);
	deck.at = 12.0;
	loop.out (deck);   // off

	deck.at = 18.0;
	loop.out (deck);
	EXPECT_TRUE (deck.looping);
	EXPECT_DOUBLE_EQ (deck.in, 10.0) << "the same IN";
	EXPECT_DOUBLE_EQ (deck.out, 18.0) << "the new OUT";
	EXPECT_DOUBLE_EQ (deck.at, 10.0);

	loop.out (deck);
	EXPECT_FALSE (deck.looping) << "and off again";
}

TEST_F (DeckLoopTest, OutAgainIsOnTheGridToo)
{
	deck.beat = 0.5;
	loopFrom (10.0, 14.0);
	deck.at = 12.0;
	loop.out (deck);
	deck.at = 17.9;
	loop.out (deck);
	EXPECT_DOUBLE_EQ (deck.in, 10.0);
	EXPECT_DOUBLE_EQ (deck.out, 18.0);
}

// LOOP keeps its role: after OUT left the loop it goes back into that loop.
TEST_F (DeckLoopTest, LoopReloopsTheLoopOutLeft)
{
	loopFrom (10.0, 14.0);
	deck.at = 12.0;
	loop.out (deck);
	EXPECT_TRUE (loop.canReloop (deck));

	deck.at = 30.0;
	loop.toggle (deck);
	EXPECT_TRUE (deck.looping);
	EXPECT_DOUBLE_EQ (deck.in, 10.0);
	EXPECT_DOUBLE_EQ (deck.out, 14.0);
	EXPECT_DOUBLE_EQ (deck.at, 10.0);
}

// A loop set elsewhere (the overview drag) is left by OUT as well; with no
// mark a second OUT has nothing to close from.
TEST_F (DeckLoopTest, OutLeavesALoopSetElsewhereAndWithoutAMarkDoesNoMore)
{
	deck.setLoop (3.0, 5.0);
	deck.at = 4.0;
	loop.out (deck);
	EXPECT_FALSE (deck.looping);
	deck.at = 8.0;
	loop.out (deck);
	EXPECT_FALSE (deck.looping);
}

// The SCS.3d's one key stays IN then OUT: after a loop the next press marks
// anew, it does not leave the loop.
TEST_F (DeckLoopTest, OneKeyMarksAnewAfterALoop)
{
	deck.at = 10.0;
	loop.inOrOut (deck);
	deck.at = 14.0;
	loop.inOrOut (deck);
	ASSERT_TRUE (deck.looping);
	deck.at = 11.0;
	loop.inOrOut (deck);
	EXPECT_TRUE (deck.looping) << "a new mark, the loop plays on";
	deck.at = 12.0;
	loop.inOrOut (deck);
	EXPECT_DOUBLE_EQ (deck.in, 11.0);
	EXPECT_DOUBLE_EQ (deck.out, 12.0);
}
