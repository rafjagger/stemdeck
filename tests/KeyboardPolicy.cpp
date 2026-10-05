#include <gtest/gtest.h>

#include "KeyboardPolicy.h"

using Command = KeyboardPolicy::Command;

TEST (KeyboardPolicy, ATextFieldTakingFocusShowsTheKeyboard)
{
	KeyboardPolicy policy { true };
	EXPECT_EQ (policy.focusChanged (true), Command::show);
}

TEST (KeyboardPolicy, FocusLeavingTheTextFieldsHidesOnlyAfterAPause)
{
	KeyboardPolicy policy { true };
	policy.focusChanged (true);
	EXPECT_EQ (policy.focusChanged (false), Command::hideSoon);
	EXPECT_EQ (policy.hideDue(), Command::hide);
}

// A hop from one text field to the next must not flash the keyboard away.
TEST (KeyboardPolicy, AHopBetweenTextFieldsKeepsTheKeyboard)
{
	KeyboardPolicy policy { true };
	policy.focusChanged (true);
	policy.focusChanged (false);
	EXPECT_EQ (policy.focusChanged (true), Command::show);
	EXPECT_EQ (policy.hideDue(), Command::none);
}

TEST (KeyboardPolicy, AHideIsDueOnlyOnce)
{
	KeyboardPolicy policy { true };
	policy.focusChanged (true);
	policy.focusChanged (false);
	policy.hideDue();
	EXPECT_EQ (policy.hideDue(), Command::none);
}

// Focus wandering between buttons never had a text field: a keyboard opened
// with KEYS stays.
TEST (KeyboardPolicy, FocusMovingOutsideTextFieldsLeavesTheKeyboardAlone)
{
	KeyboardPolicy policy { true };
	EXPECT_EQ (policy.focusChanged (false), Command::none);
	EXPECT_EQ (policy.hideDue(), Command::none);
}

TEST (KeyboardPolicy, KeysToggles)
{
	KeyboardPolicy policy { true };
	EXPECT_EQ (policy.keysPressed(), Command::toggle);
	EXPECT_EQ (policy.keysPressed(), Command::toggle);
}

// KEYS pressed while a hide is pending: the hand decided, the timer does not
// undo it a moment later.
TEST (KeyboardPolicy, KeysCancelsAPendingHide)
{
	KeyboardPolicy policy { true };
	policy.focusChanged (true);
	policy.focusChanged (false);
	EXPECT_EQ (policy.keysPressed(), Command::toggle);
	EXPECT_EQ (policy.hideDue(), Command::none);
}

TEST (KeyboardPolicy, WithoutOnboardKeysOnlyGivesAHint)
{
	KeyboardPolicy policy { false };
	EXPECT_EQ (policy.keysPressed(), Command::hint);
	EXPECT_EQ (policy.focusChanged (true), Command::none);
	EXPECT_EQ (policy.focusChanged (false), Command::none);
	EXPECT_EQ (policy.hideDue(), Command::none);
}

TEST (KeyboardPolicy, OnboardFailingToStartMakesItUnavailable)
{
	KeyboardPolicy policy { true };
	policy.focusChanged (true);
	policy.focusChanged (false);
	policy.setAvailable (false);
	EXPECT_FALSE (policy.isAvailable());
	EXPECT_EQ (policy.hideDue(), Command::none);
	EXPECT_EQ (policy.keysPressed(), Command::hint);
}
