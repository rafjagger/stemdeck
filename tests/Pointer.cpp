#include <gtest/gtest.h>

#include "Pointer.h"

// The "Show the pointer" switch in Settings, as the settings file keeps it.
TEST (Pointer, HiddenUntilSwitchedOn)
{
	EXPECT_FALSE (pointer::shown ("")) << "a fresh StemDeck never draws the pointer";
	EXPECT_FALSE (pointer::shownByDefault);
}

TEST (Pointer, TheChoiceComesBackAfterARestart)
{
	EXPECT_TRUE (pointer::shown (pointer::stored (true)));
	EXPECT_FALSE (pointer::shown (pointer::stored (false)));
}

TEST (Pointer, ReadsWhatJuceWritesForABool)
{
	EXPECT_TRUE (pointer::shown ("1"));
	EXPECT_FALSE (pointer::shown ("0"));
}

TEST (Pointer, VisibilityStartsHiddenAndTheSwitchShowsIt)
{
	pointer::Visibility visibility;
	EXPECT_FALSE (visibility.isShown());
	visibility.set (true);
	EXPECT_TRUE (visibility.isShown());
	visibility.set (false);
	EXPECT_FALSE (visibility.isShown());
}
