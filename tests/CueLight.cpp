#include <gtest/gtest.h>

#include "CueLight.h"

using cuelight::CueLight;
using cuelight::blinkPhaseOn;
using cuelight::cueLight;

TEST (CueLight, HeldIsSteadyWhateverElseIsTrue)
{
	EXPECT_EQ (cueLight (true, true, true, 5.0, 1.0), CueLight::steady);
	EXPECT_EQ (cueLight (true, false, true, 5.0, 1.0), CueLight::steady);
}

TEST (CueLight, PausedOnTheCuePointIsSteady)
{
	EXPECT_EQ (cueLight (true, false, false, 1.0, 1.0), CueLight::steady);
	EXPECT_EQ (cueLight (true, false, false, 1.009, 1.0), CueLight::steady);
	EXPECT_EQ (cueLight (true, false, false, 0.991, 1.0), CueLight::steady);
}

TEST (CueLight, PausedAwayFromTheCuePointBlinks)
{
	EXPECT_EQ (cueLight (true, false, false, 1.011, 1.0), CueLight::blink);
	EXPECT_EQ (cueLight (true, false, false, 30.0, 1.0), CueLight::blink);
}

TEST (CueLight, PlayingIsOffUnlessHeld)
{
	EXPECT_EQ (cueLight (true, true, false, 1.0, 1.0), CueLight::off);
	EXPECT_EQ (cueLight (true, true, false, 30.0, 1.0), CueLight::off);
}

TEST (CueLight, NothingLoadedIsOff)
{
	EXPECT_EQ (cueLight (false, false, false, 0.0, 0.0), CueLight::off);
	EXPECT_EQ (cueLight (false, false, true, 0.0, 0.0), CueLight::off);
}

TEST (CueLight, BlinksAtTwoHertzFromTime)
{
	EXPECT_TRUE (blinkPhaseOn (0.0));
	EXPECT_TRUE (blinkPhaseOn (0.249));
	EXPECT_FALSE (blinkPhaseOn (0.251));
	EXPECT_FALSE (blinkPhaseOn (0.499));
	EXPECT_TRUE (blinkPhaseOn (0.501));
	EXPECT_TRUE (blinkPhaseOn (1000.1));
	EXPECT_FALSE (blinkPhaseOn (1000.3));
}
