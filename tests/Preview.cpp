#include <gtest/gtest.h>

#include <cmath>

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
	EXPECT_DOUBLE_EQ (moment.barSeconds, 2.0);
}

TEST (Preview, ABarOfDeckTimeFollowsTheTempoFader)
{
	auto deck = deckAt (8.5);
	deck.speed = 1.25;
	EXPECT_DOUBLE_EQ (preview::momentOf ({ deck, {} }, -1).barSeconds, 1.6);
	EXPECT_DOUBLE_EQ (preview::momentOf ({ preview::DeckState {}, {} }, -1).barSeconds, 0.0) << "nothing heard";
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
	moment.barSeconds = 2.0;
	const auto t = 10.0;   // all within one bar of deck time

	EXPECT_TRUE (gate.shouldSend (moment, t));
	EXPECT_FALSE (gate.shouldSend (moment, t)) << "the same bar: nothing new";

	++moment.bar;
	EXPECT_TRUE (gate.shouldSend (moment, t)) << "the downbeat";

	moment.deck = 1;
	EXPECT_TRUE (gate.shouldSend (moment, t)) << "the other deck";

	++moment.generation;
	EXPECT_TRUE (gate.shouldSend (moment, t)) << "a load";

	moment.ahead.barsUntilNext = -1;
	EXPECT_TRUE (gate.shouldSend (moment, t)) << "a loop";

	moment.bar = 2;
	EXPECT_TRUE (gate.shouldSend (moment, t)) << "a jump back";
}

// What the preview says does not depend on the tempo fader: a sweep or a
// sync follow alone sends nothing.
TEST (Preview, ThePitchAloneSendsNothing)
{
	preview::Gate gate;
	auto deck = deckAt (8.5);
	EXPECT_TRUE (gate.shouldSend (preview::momentOf ({ deck, {} }, -1), 0.0));

	for (auto speed : { 0.92, 1.04, 1.08 })
	{
		deck.speed = speed;
		EXPECT_FALSE (gate.shouldSend (preview::momentOf ({ deck, {} }, -1), 0.1)) << speed;
	}
}

namespace
{
	// Deck A held in a loop of `loopBars` bars from bar 1, ticked at 32 Hz for
	// `seconds` of wall time; the times at which the gate lets a preview out.
	std::vector<double> sendsInALoop (double loopBars, double seconds)
	{
		const auto barSeconds = 2.0, loopStart = 0.25 + barSeconds;
		const auto loopLength = loopBars * barSeconds;
		auto deck = deckAt (loopStart);
		deck.loopSeconds = std::make_pair (loopStart, loopStart + loopLength);

		preview::Gate gate;
		std::vector<double> sent;
		for (int tick = 0; tick < (int) (seconds * 32.0); ++tick)
		{
			const auto now = tick / 32.0;
			deck.position = loopStart + std::fmod (now, loopLength);
			if (gate.shouldSend (preview::momentOf ({ deck, {} }, -1), now))
				sent.push_back (now);
		}
		return sent;
	}
}

// Motion counts a preview stale after a few bars without one; a loop of a
// bar or less never turns the bar, so a bar of deck time sends again.
TEST (Preview, AOneBarLoopKeepsSendingOnceABar)
{
	EXPECT_EQ (sendsInALoop (1.0, 8.0), (std::vector<double> { 0.0, 2.0, 4.0, 6.0 }));
}

TEST (Preview, AOneBeatLoopKeepsSendingOnceABar)
{
	EXPECT_EQ (sendsInALoop (0.25, 8.0), (std::vector<double> { 0.0, 2.0, 4.0, 6.0 }));
}

TEST (Preview, PlayingFreelySendsOnTheDownbeatsOnly)
{
	preview::Gate gate;
	auto deck = deckAt (0.25);
	std::vector<double> sent;
	for (int tick = 0; tick < 8 * 32; ++tick)
	{
		const auto now = tick / 32.0;
		deck.position = 0.25 + now;
		if (gate.shouldSend (preview::momentOf ({ deck, {} }, -1), now))
			sent.push_back (now);
	}
	EXPECT_EQ (sent, (std::vector<double> { 0.0, 2.0, 4.0, 6.0 }));
}

TEST (Preview, NoneIsSentOnceNotEveryTick)
{
	preview::Gate gate;
	const preview::Moment silent;   // deck -1, "none"
	EXPECT_TRUE (gate.shouldSend (silent, 0.0));
	EXPECT_FALSE (gate.shouldSend (silent, 100.0)) << "no deck heard: no bar to repeat on";
}

// The one moved by hand while the deck plays.
TEST (Preview, TheOneMovedByHandSendsAtOnce)
{
	preview::Gate gate;
	auto deck = deckAt (8.5);
	EXPECT_TRUE (gate.shouldSend (preview::momentOf ({ deck, {} }, -1), 0.0));
	EXPECT_FALSE (gate.shouldSend (preview::momentOf ({ deck, {} }, -1), 0.0));

	deck.firstBeat += 0.5;   // 1>: the one a beat later
	const auto moved = preview::momentOf ({ deck, {} }, -1);
	EXPECT_EQ (moved.bar, 3);
	EXPECT_TRUE (gate.shouldSend (moved, 0.0)) << "not at the next downbeat: now";
}
