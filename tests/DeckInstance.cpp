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
