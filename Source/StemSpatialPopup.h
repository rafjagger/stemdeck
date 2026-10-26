// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "DeckSpatial.h"

#include <juce_gui_basics/juce_gui_basics.h>

// One stem's place in its deck's spatial engine: 3D, direction (azimuth,
// elevation) and width, four sliders writing DeckSpatial::Controls. Shown in a
// CallOutBox pointing at the stem's row in the mixer. The ranges are IEM's
// StereoEncoder's; the session picks the values up on its next gather.
class StemSpatialPopup : public juce::Component
{
public:
	StemSpatialPopup (DeckSpatial::Controls&, int stem);

	juce::Slider& threeD() { return threeDSlider; }
	juce::Slider& azimuth() { return azimuthSlider; }
	juce::Slider& elevation() { return elevationSlider; }
	juce::Slider& width() { return widthSlider; }

	// Opens the call-out at `anchor`, sized from the window it sits in; it is
	// dismissed by a click outside and deletes itself.
	static void show (juce::Component& anchor, DeckSpatial::Controls&, int stem);

	void resized() override;

private:
	juce::Slider threeDSlider, azimuthSlider, elevationSlider, widthSlider;
	juce::Label threeDLabel, azimuthLabel, elevationLabel, widthLabel;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StemSpatialPopup)
};
