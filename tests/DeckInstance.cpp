// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "DeckInstance.h"

TEST (DeckInstance, NamesFollowTheLetter)
{
	const DeckInstance a { 'A' };
	EXPECT_EQ (a.jackClientName(), "StemDeck-A");
	EXPECT_EQ (a.sessionDir(), "A");
	EXPECT_EQ (a.oscDeck(), 1);
	EXPECT_EQ (a.title(), "StemDeck A");

	const DeckInstance b { 'B' };
	EXPECT_EQ (b.jackClientName(), "StemDeck-B");
	EXPECT_EQ (b.sessionDir(), "B");
	EXPECT_EQ (b.oscDeck(), 2);
	EXPECT_EQ (b.title(), "StemDeck B");
}

// Two instances on one host share the settings file, so the player number on
// the link comes from the letter: the receiver drops its own number's beats.
// A is player 6 (the two-deck app's), B 5: a CDJ-3000 numbers players 1-6
// (manual p. 113), and a number outside that range is one no CDJ or DJM
// expects.
TEST (DeckInstance, ProLinkPlayerFollowsTheLetter)
{
	EXPECT_EQ ((DeckInstance { 'A' }).proLinkDevice(), 6);
	EXPECT_EQ ((DeckInstance { 'B' }).proLinkDevice(), 5);
}
