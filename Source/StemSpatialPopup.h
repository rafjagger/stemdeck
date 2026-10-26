// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "DeckSpatial.h"
#include "StemViewer.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

class DeckMotion;

// One stem's place in its deck's spatial engine: 3D, direction (azimuth,
// elevation) and width, four sliders writing DeckSpatial::Controls, and the
// viewer showing and dragging the same direction and width. With the deck's
// DeckMotion it also carries the one-shot pads (ORBIT, PING, RISE, SCAT; idle,
// armed, running), the length keys and SHIFT (the next press starts now, then
// SHIFT lets go). Shown in a CallOutBox pointing at the stem's row in the
// mixer. The ranges are IEM's StereoEncoder's; the session picks the values
// up on its next gather.
class StemSpatialPopup : public juce::Component, private juce::Timer
{
public:
	StemSpatialPopup (DeckSpatial::Controls&, int stem, DeckMotion* = nullptr);
	~StemSpatialPopup() override;

	juce::Slider& threeD() { return threeDSlider; }
	juce::Slider& azimuth() { return azimuthSlider; }
	juce::Slider& elevation() { return elevationSlider; }
	juce::Slider& width() { return widthSlider; }
	StemViewer& viewer() { return stemViewer; }
	juce::TextButton& pad (motionlite::Shape shape) { return pads[(size_t) shape]; }
	juce::TextButton& lengthDown() { return shorter; }
	juce::TextButton& lengthUp() { return longer; }
	juce::TextButton& shift() { return shiftKey; }
	juce::String lengthText() const { return lengthLabel.getText(); }

	void setStemColour (juce::Colour);
	// Reads what the audio thread last did into the pads, the length and the
	// viewer; a timer calls it while the call-out is open.
	void refresh();

	// Opens the call-out at `anchor`, sized from the window it sits in; it is
	// dismissed by a click outside and deletes itself.
	static void show (juce::Component& anchor, DeckSpatial::Controls&, DeckMotion*, int stem);

	void resized() override;

private:
	void timerCallback() override { refresh(); }
	void pressPad (motionlite::Shape);
	void homeDragged (spatial::Pose);
	spatial::Pose homeNow() const;
	void lookOfPad (motionlite::Shape);

	DeckSpatial::Controls& controls;
	const size_t stemIndex;
	DeckMotion* motion;
	juce::Colour stemColour;

	juce::Slider threeDSlider, azimuthSlider, elevationSlider, widthSlider;
	juce::Label threeDLabel, azimuthLabel, elevationLabel, widthLabel;
	StemViewer stemViewer;
	std::array<juce::TextButton, 4> pads;
	juce::TextButton shorter, longer, shiftKey;
	juce::Label lengthLabel;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StemSpatialPopup)
};
