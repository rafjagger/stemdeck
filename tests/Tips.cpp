#include <gtest/gtest.h>

#include "Tips.h"

// The tips (tooltips) switch in Settings, as the settings file keeps it.
TEST (Tips, OnUntilSwitchedOff)
{
	EXPECT_TRUE (tips::shown ("")) << "a fresh StemDeck shows them";
	EXPECT_TRUE (tips::shownByDefault);
}

TEST (Tips, TheChoiceComesBackAfterARestart)
{
	EXPECT_FALSE (tips::shown (tips::stored (false)));
	EXPECT_TRUE (tips::shown (tips::stored (true)));
}

TEST (Tips, ReadsWhatJuceWritesForABool)
{
	// PropertiesFile::setValue (key, bool) stores a var: "1" or "0".
	EXPECT_TRUE (tips::shown ("1"));
	EXPECT_FALSE (tips::shown ("0"));
}
