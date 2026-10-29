// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include "DoublePress.h"

TEST (DoublePress, ASecondPressWithinTheWindowIsADouble)
{
	DoublePress press;
	EXPECT_FALSE (press.press (10.0));
	EXPECT_TRUE (press.press (10.0 + DoublePress::windowSeconds * 0.9));
}

TEST (DoublePress, ASecondPressAfterTheWindowStartsAgain)
{
	DoublePress press;
	EXPECT_FALSE (press.press (10.0));
	EXPECT_FALSE (press.press (10.0 + DoublePress::windowSeconds * 1.1));
}

// A double is used up: a third quick press is a first one again, not another double.
TEST (DoublePress, ADoubleIsUsedUp)
{
	DoublePress press;
	EXPECT_FALSE (press.press (1.0));
	EXPECT_TRUE (press.press (1.1));
	EXPECT_FALSE (press.press (1.2));
	EXPECT_TRUE (press.press (1.3));
}
