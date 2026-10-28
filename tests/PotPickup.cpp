// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "PotPickup.h"

TEST (PotPickup, ActsOnlyAfterCrossing)
{
	PotPickup pot;
	pot.rearm (0.5);
	EXPECT_FALSE (pot.feed (0.2));
	EXPECT_FALSE (pot.feed (0.4));
	EXPECT_FALSE (pot.pickedUp());
	EXPECT_DOUBLE_EQ (pot.ghost(), 0.4);
	const auto crossed = pot.feed (0.55);
	ASSERT_TRUE (crossed);
	EXPECT_DOUBLE_EQ (*crossed, 0.55);
	EXPECT_TRUE (pot.pickedUp());
	ASSERT_TRUE (pot.feed (0.9));
	EXPECT_DOUBLE_EQ (*pot.feed (0.1), 0.1);
}

TEST (PotPickup, CrossingFromAbove)
{
	PotPickup pot;
	pot.rearm (0.5);
	EXPECT_FALSE (pot.feed (0.9));
	EXPECT_FALSE (pot.feed (0.6));
	EXPECT_TRUE (pot.feed (0.45));
}

TEST (PotPickup, WithinOnePercentPicksUp)
{
	PotPickup pot;
	pot.rearm (0.5);
	EXPECT_TRUE (pot.feed (0.505));

	pot.rearm (0.5);
	EXPECT_FALSE (pot.feed (0.52));
}

TEST (PotPickup, RearmDropsThePickup)
{
	PotPickup pot;
	pot.rearm (0.5);
	pot.feed (0.5);
	ASSERT_TRUE (pot.pickedUp());
	pot.rearm (0.8);
	EXPECT_FALSE (pot.pickedUp());
	EXPECT_FALSE (pot.feed (0.3));
	EXPECT_FALSE (pot.feed (0.5)) << "the reading before rearm does not count as one side";
	EXPECT_TRUE (pot.feed (0.85));
}

TEST (PotPickup, RearmWithTheKnownPotPlaceKeepsItAsASide)
{
	PotPickup pot;
	pot.rearm (0.5, 0.2);
	EXPECT_DOUBLE_EQ (pot.ghost(), 0.2);
	EXPECT_FALSE (pot.pickedUp());
	EXPECT_TRUE (pot.feed (0.6)) << "one report across the value is enough";
}

TEST (PotPickup, AnUntouchedPotShowsTheStoredValueAsGhost)
{
	PotPickup pot;
	pot.rearm (0.3);
	EXPECT_DOUBLE_EQ (pot.ghost(), 0.3);
}
