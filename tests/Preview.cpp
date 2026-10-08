#include <gtest/gtest.h>

#include "Preview.h"

using sections::Bar;
using sections::Section;

namespace
{
	// groove groove breakdown breakdown build drop
	std::vector<Bar> aSet()
	{
		return { { Section::groove, 0.8f }, { Section::groove, 0.8f }, { Section::breakdown, 0.2f },
				 { Section::breakdown, 0.2f }, { Section::build, 0.5f }, { Section::drop, 1.0f } };
	}

	// Deck A playing at 120 BPM (a bar 2 s), the downbeat at 0.25 s.
	preview::DeckState deckAt (double position)
	{
		preview::DeckState deck;
		deck.view = { true, 1.0f };
		deck.generation = 3;
		deck.bpm = 120.0;
		deck.firstBeat = 0.25;
		deck.position = position;
		deck.bars = aSet();
		return deck;
	}
}

TEST (Preview, WhatComesNextAndWhen)
{
	const auto ahead = preview::aheadAt (aSet(), 0.0, std::nullopt);
	EXPECT_EQ (ahead.section, "groove");
	EXPECT_EQ (ahead.next, "breakdown");
	EXPECT_EQ (ahead.barsUntilNext, 2);
	EXPECT_FLOAT_EQ (ahead.energy, 0.8f);
}

TEST (Preview, CountsFromTheBarItIsIn)
{
	const auto ahead = preview::aheadAt (aSet(), 1.7, std::nullopt);
	EXPECT_EQ (ahead.section, "groove");
	EXPECT_EQ (ahead.barsUntilNext, 1);

	const auto build = preview::aheadAt (aSet(), 4.0, std::nullopt);
	EXPECT_EQ (build.section, "build");
	EXPECT_EQ (build.next, "drop");
	EXPECT_EQ (build.barsUntilNext, 1);
}

TEST (Preview, TheLastSectionRunsToTheEnd)
{
	const auto ahead = preview::aheadAt (aSet(), 5.2, std::nullopt);
	EXPECT_EQ (ahead.section, "drop");
	EXPECT_EQ (ahead.next, "end");
	EXPECT_EQ (ahead.barsUntilNext, 1);
}

TEST (Preview, BeforeTheDownbeatIsBarZeroAndPastTheEndIsNone)
{
	EXPECT_EQ (preview::aheadAt (aSet(), -0.3, std::nullopt).section, "groove");
	EXPECT_EQ (preview::aheadAt (aSet(), 6.0, std::nullopt), preview::Ahead {});
	EXPECT_EQ (preview::aheadAt ({}, 0.0, std::nullopt), preview::Ahead {}) << "not analysed: none";
}

TEST (Preview, ALoopBeforeTheChangeHoldsItOff)
{
	const auto ahead = preview::aheadAt (aSet(), 1.2, preview::LoopBars { 1.0, 2.0 });
	EXPECT_EQ (ahead.section, "groove");
	EXPECT_EQ (ahead.next, "groove");
	EXPECT_EQ (ahead.barsUntilNext, -1);
}

TEST (Preview, ALoopPastTheChangeDoesNot)
{
	const auto ahead = preview::aheadAt (aSet(), 1.2, preview::LoopBars { 1.0, 4.0 });
	EXPECT_EQ (ahead.next, "breakdown");
	EXPECT_EQ (ahead.barsUntilNext, 1);
}

// The player plays into a loop that lies ahead (it reads on to the loop's
// end and turns back), so a loop ahead that ends before the change holds it
// off already.
TEST (Preview, ALoopAheadIsEnteredAndHoldsTheChangeOff)
{
	const auto ahead = preview::aheadAt (aSet(), 0.5, preview::LoopBars { 1.0, 2.0 });
	EXPECT_EQ (ahead.next, "groove");
	EXPECT_EQ (ahead.barsUntilNext, -1);
}

TEST (Preview, ALoopAheadPastTheChangeLetsItCome)
{
	const auto ahead = preview::aheadAt (aSet(), 0.5, preview::LoopBars { 3.0, 4.0 });
	EXPECT_EQ (ahead.next, "breakdown");
	EXPECT_EQ (ahead.barsUntilNext, 2);
}

// A jump past the loop's end leaves the loop set, but the player plays on
// freely from there.
TEST (Preview, APlayheadPastTheLoopPlaysOn)
{
	const auto ahead = preview::aheadAt (aSet(), 1.2, preview::LoopBars { 0.0, 1.0 });
	EXPECT_EQ (ahead.next, "breakdown");
	EXPECT_EQ (ahead.barsUntilNext, 1);
}

TEST (Preview, TheAudibleDeck)
{
	using preview::DeckView;
	const DeckView playingUp { true, 1.0f }, playingDown { true, 0.0f }, stopped { false, 1.0f };
	const DeckView half { true, 0.5f }, whisper { true, 0.03f };

	EXPECT_EQ (preview::audibleDeck ({ playingUp, stopped }, -1), 0);
	EXPECT_EQ (preview::audibleDeck ({ playingDown, playingUp }, -1), 1) << "a closed fader is not heard";
	EXPECT_EQ (preview::audibleDeck ({ stopped, stopped }, 0), -1);
	EXPECT_EQ (preview::audibleDeck ({ playingUp, playingUp }, 1), 1) << "both: the master";
	EXPECT_EQ (preview::audibleDeck ({ playingUp, half }, -1), 0) << "both, no master: the louder";
	EXPECT_EQ (preview::audibleDeck ({ half, playingUp }, -1), 1);
	EXPECT_EQ (preview::audibleDeck ({ playingUp, playingUp }, -1), 0) << "a tie: A";
	EXPECT_EQ (preview::audibleDeck ({ whisper, stopped }, -1), -1) << "below -30 dB";
}

TEST (Preview, TheMomentOfTheDeckThatIsHeard)
{
	const auto moment = preview::momentOf ({ deckAt (8.5), preview::DeckState {} }, -1);
	EXPECT_EQ (moment.deck, 0);
	EXPECT_EQ (moment.generation, 3);
	EXPECT_EQ (moment.bar, 4) << "(8.5 - 0.25) / 2";
	EXPECT_EQ (moment.ahead.section, "build");
	EXPECT_EQ (moment.ahead.next, "drop");
	EXPECT_EQ (moment.speedPermille, 1000);
}

TEST (Preview, TheBarTurnsOnTheDownbeat)
{
	EXPECT_EQ (preview::momentOf ({ deckAt (2.24), {} }, -1).bar, 0);
	EXPECT_EQ (preview::momentOf ({ deckAt (2.25), {} }, -1).bar, 1);
}

TEST (Preview, ALoopInSecondsBecomesBars)
{
	auto deck = deckAt (2.5);                       // bar 1.125
	deck.loopSeconds = std::make_pair (2.25, 4.25); // bars 1-2
	EXPECT_EQ (preview::momentOf ({ deck, {} }, -1).ahead.barsUntilNext, -1);
}

TEST (Preview, NothingHeardOrNothingKnownIsNone)
{
	EXPECT_EQ (preview::momentOf ({ preview::DeckState {}, preview::DeckState {} }, -1).ahead, preview::Ahead {});

	auto noGrid = deckAt (8.5);
	noGrid.bpm = 0.0;
	EXPECT_EQ (preview::momentOf ({ noGrid, {} }, -1).ahead, preview::Ahead {});

	auto notAnalysed = deckAt (8.5);
	notAnalysed.bars.clear();
	const auto moment = preview::momentOf ({ notAnalysed, {} }, -1);
	EXPECT_EQ (moment.deck, 0);
	EXPECT_EQ (moment.ahead, preview::Ahead {});
}

TEST (Preview, SentOnEveryDownbeatAndOnAChange)
{
	preview::Gate gate;
	preview::Moment moment;
	moment.deck = 0;
	moment.bar = 4;

	EXPECT_TRUE (gate.shouldSend (moment));
	EXPECT_FALSE (gate.shouldSend (moment)) << "the same bar: nothing new";

	++moment.bar;
	EXPECT_TRUE (gate.shouldSend (moment)) << "the downbeat";

	moment.speedPermille = 1040;
	EXPECT_TRUE (gate.shouldSend (moment)) << "the pitch";

	moment.deck = 1;
	EXPECT_TRUE (gate.shouldSend (moment)) << "the other deck";

	++moment.generation;
	EXPECT_TRUE (gate.shouldSend (moment)) << "a load";

	moment.ahead.barsUntilNext = -1;
	EXPECT_TRUE (gate.shouldSend (moment)) << "a loop";

	moment.bar = 2;
	EXPECT_TRUE (gate.shouldSend (moment)) << "a jump back";
}

TEST (Preview, NoneIsSentOnceNotEveryTick)
{
	preview::Gate gate;
	const preview::Moment silent;   // deck -1, "none"
	EXPECT_TRUE (gate.shouldSend (silent));
	EXPECT_FALSE (gate.shouldSend (silent));
}

// The one moved by hand while the deck plays.
TEST (Preview, TheOneMovedByHandSendsAtOnce)
{
	preview::Gate gate;
	auto deck = deckAt (8.5);
	EXPECT_TRUE (gate.shouldSend (preview::momentOf ({ deck, {} }, -1)));
	EXPECT_FALSE (gate.shouldSend (preview::momentOf ({ deck, {} }, -1)));

	deck.firstBeat += 0.5;   // 1>: the one a beat later
	const auto moved = preview::momentOf ({ deck, {} }, -1);
	EXPECT_EQ (moved.bar, 3);
	EXPECT_TRUE (gate.shouldSend (moved)) << "not at the next downbeat: now";
}
