// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "StemSpatialPopup.h"

// A popup is a component: it needs JUCE's GUI initialised, no X window.
TEST (StemSpatialPopup, SlidersWriteTheControls)
{
	juce::ScopedJuceInitialiser_GUI init;
	DeckSpatial::Controls controls;
	StemSpatialPopup popup (controls, 2);
	popup.setSize (400, 200);

	popup.threeD().setValue (0.75, juce::sendNotificationSync);
	popup.azimuth().setValue (-90.0, juce::sendNotificationSync);
	popup.elevation().setValue (30.0, juce::sendNotificationSync);
	popup.width().setValue (200.0, juce::sendNotificationSync);

	EXPECT_FLOAT_EQ (controls.threeD[2].load(), 0.75f);
	EXPECT_FLOAT_EQ (controls.azimuth[2].load(), -90.0f);
	EXPECT_FLOAT_EQ (controls.elevation[2].load(), 30.0f);
	EXPECT_FLOAT_EQ (controls.width[2].load(), 200.0f);

	for (int other : { 0, 1, 3 })   // only that stem's
	{
		EXPECT_FLOAT_EQ (controls.threeD[(size_t) other].load(), 0.0f);
		EXPECT_FLOAT_EQ (controls.width[(size_t) other].load(), 60.0f);
	}
}

TEST (StemSpatialPopup, StartsFromTheControlsAndKeepsIEMsRanges)
{
	juce::ScopedJuceInitialiser_GUI init;
	DeckSpatial::Controls controls;
	controls.azimuth[1] = 45.0f;
	controls.width[1] = 120.0f;
	StemSpatialPopup popup (controls, 1);

	EXPECT_DOUBLE_EQ (popup.azimuth().getValue(), 45.0);
	EXPECT_DOUBLE_EQ (popup.width().getValue(), 120.0);
	EXPECT_DOUBLE_EQ (popup.threeD().getMaximum(), 1.0);
	EXPECT_DOUBLE_EQ (popup.azimuth().getMinimum(), -180.0);
	EXPECT_DOUBLE_EQ (popup.elevation().getMaximum(), 90.0);
	EXPECT_DOUBLE_EQ (popup.width().getMaximum(), 360.0);

	// Opening it writes nothing back.
	EXPECT_FLOAT_EQ (controls.azimuth[1].load(), 45.0f);
}

TEST (StemSpatialPopup, PaintsEveryState)
{
	juce::ScopedJuceInitialiser_GUI init;
	DeckSpatial::Controls controls;
	for (const auto threeD : { 0.0f, 1.0f })
	{
		controls.threeD[0] = threeD;
		StemSpatialPopup popup (controls, 0);
		popup.setSize (400, 200);
		const auto image = popup.createComponentSnapshot (popup.getLocalBounds());
		EXPECT_EQ (image.getWidth(), 400);
	}
}
