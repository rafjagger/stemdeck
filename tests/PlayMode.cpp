// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "PlayMode.h"

using playmode::Mode;
using playmode::Plan;

namespace
{
	// The library as the panel shows it: ids in the folder's sort order.
	const std::vector<std::string> shown { "a", "b", "c" };

	// A deck playing set `current`, `remaining` seconds before its end.
	playmode::Deck deckAt (const std::string& current, double remaining, Mode mode = Mode::continuous)
	{
		playmode::Deck deck;
		deck.mode = mode;
		deck.current = current;
		deck.length = 300.0;
		deck.position = deck.length - remaining;
		deck.playing = true;
		return deck;
	}
}

// One play mode, as a CDJ has one: turning REPEAT on turns CONTINUE off, and back.
TEST (PlayMode, RepeatAndContinueAreExclusive)
{
	EXPECT_EQ (playmode::toggledRepeat (Mode::single), Mode::repeat);
	EXPECT_EQ (playmode::toggledRepeat (Mode::continuous), Mode::repeat);
	EXPECT_EQ (playmode::toggledRepeat (Mode::repeat), Mode::single);
	EXPECT_EQ (playmode::toggledContinue (Mode::single), Mode::continuous);
	EXPECT_EQ (playmode::toggledContinue (Mode::repeat), Mode::continuous);
	EXPECT_EQ (playmode::toggledContinue (Mode::continuous), Mode::single);
}

// A session with both stored (by hand, or a newer file) comes back as one mode: REPEAT.
TEST (PlayMode, StoredFlagsComeBackAsOneMode)
{
	EXPECT_EQ (playmode::fromFlags (false, false), Mode::single);
	EXPECT_EQ (playmode::fromFlags (true, false), Mode::repeat);
	EXPECT_EQ (playmode::fromFlags (false, true), Mode::continuous);
	EXPECT_EQ (playmode::fromFlags (true, true), Mode::repeat);
}

TEST (PlayMode, NextIsTheOneBelowInTheShownList)
{
	EXPECT_EQ (playmode::nextIn (shown, "a"), std::optional<std::size_t> (1));
	EXPECT_EQ (playmode::nextIn (shown, "b"), std::optional<std::size_t> (2));
	EXPECT_FALSE (playmode::nextIn (shown, "c")) << "the end of the list: nothing, no wrap";
	EXPECT_FALSE (playmode::nextIn (shown, "x")) << "a set the list does not show has no next";
	EXPECT_FALSE (playmode::nextIn ({}, "a"));
}

TEST (PlayMode, StagesTheNextSetTenSecondsBeforeTheEnd)
{
	EXPECT_EQ (playmode::plan (deckAt ("a", 10.5), shown).action, Plan::Action::none) << "too early";

	const auto due = playmode::plan (deckAt ("a", 10.0), shown);
	EXPECT_EQ (due.action, Plan::Action::stage);
	EXPECT_EQ (due.id, "b");

	auto staged = deckAt ("a", 4.0);
	staged.staged = "b";
	EXPECT_EQ (playmode::plan (staged, shown).action, Plan::Action::none) << "staged once";
}

// The list changed under a staged set (a new sort, a search): the one now next is staged.
TEST (PlayMode, AStaleStagedSetIsReplaced)
{
	auto deck = deckAt ("a", 4.0);
	deck.staged = "c";
	const auto plan = playmode::plan (deck, shown);
	EXPECT_EQ (plan.action, Plan::Action::stage);
	EXPECT_EQ (plan.id, "b");
}

TEST (PlayMode, TheLastSetOfTheListStagesNothing)
{
	EXPECT_EQ (playmode::plan (deckAt ("c", 2.0), shown).action, Plan::Action::none);

	auto deck = deckAt ("c", 2.0);
	deck.staged = "b";   // staged before the list was sorted the other way
	EXPECT_EQ (playmode::plan (deck, shown).action, Plan::Action::unstage);
}

TEST (PlayMode, OnlyContinueStages)
{
	for (const auto mode : { Mode::single, Mode::repeat })
	{
		EXPECT_EQ (playmode::plan (deckAt ("a", 2.0, mode), shown).action, Plan::Action::none);
		auto deck = deckAt ("a", 2.0, mode);
		deck.staged = "b";   // CONTINUE was turned off after staging
		EXPECT_EQ (playmode::plan (deck, shown).action, Plan::Action::unstage);
	}
}

// A loop holds: nothing is staged while the deck loops, and what was goes.
TEST (PlayMode, ALoopHolds)
{
	auto deck = deckAt ("a", 2.0);
	deck.looping = true;
	EXPECT_EQ (playmode::plan (deck, shown).action, Plan::Action::none);
	EXPECT_FALSE (playmode::continues (deck));
	deck.staged = "b";
	EXPECT_EQ (playmode::plan (deck, shown).action, Plan::Action::unstage);
}

// Auto DJ owns the decks: CONTINUE does nothing while it runs.
TEST (PlayMode, AutoDjWins)
{
	auto deck = deckAt ("a", 2.0);
	deck.autoDj = true;
	EXPECT_FALSE (playmode::continues (deck));
	EXPECT_EQ (playmode::plan (deck, shown).action, Plan::Action::none);
	deck.staged = "b";
	EXPECT_EQ (playmode::plan (deck, shown).action, Plan::Action::unstage);
}

// A stopped deck stages too (the DJ may press PLAY in the last seconds), but
// only one that stopped at its end with nothing staged loads the next and plays.
TEST (PlayMode, ADeckThatRanOutWithNothingStagedLoadsTheNextAndPlays)
{
	auto deck = deckAt ("a", 0.0);
	deck.playing = false;
	deck.ranOut = true;
	const auto plan = playmode::plan (deck, shown);
	EXPECT_EQ (plan.action, Plan::Action::loadAndPlay);
	EXPECT_EQ (plan.id, "b");

	deck.current = "c";
	EXPECT_EQ (playmode::plan (deck, shown).action, Plan::Action::none) << "the last set: it stops, as a CDJ does";
}

TEST (PlayMode, AnEmptyDeckStagesNothing)
{
	playmode::Deck deck;
	deck.mode = Mode::continuous;
	EXPECT_EQ (playmode::plan (deck, shown).action, Plan::Action::none);
}
