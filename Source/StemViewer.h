// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "MotionLite.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

// One stem's stereo pair seen from above, as IEM StereoEncoder's display
// shows it: the sphere as a disc (front up, left left, the rim the horizon,
// straight up the middle), the direction as a dot between an L and an R dot
// placed where IEM puts them (width/2 either side, then tilted by the
// elevation). The middle dot drags azimuth and elevation, L or R the width.
// While a shot runs it only shows: the running pose bright, home as a ghost.
class StemViewer : public juce::Component
{
public:
	// The stem's own colour; the ground and the grid come from the look and feel.
	enum ColourIds { stemColourId = 0x5d3e0010 };

	struct Dots { juce::Point<float> centre, left, right; };

	StemViewer();

	std::function<void (spatial::Pose)> onHomeChanged;
	void setShown (spatial::Pose home, spatial::Pose now, motionlite::PadState);

	// Where a direction (degrees) and a pose's dots are in this component.
	juce::Point<float> pointOf (float azimuth, float elevation) const;
	Dots dotsFor (const spatial::Pose&) const;

	void paint (juce::Graphics&) override;
	void mouseDown (const juce::MouseEvent&) override;
	void mouseDrag (const juce::MouseEvent&) override;
	void mouseUp (const juce::MouseEvent&) override;

private:
	enum class Grab { none, centre, left, right };

	juce::Point<float> middle() const;
	float discRadius() const;
	float dotRadius() const;
	void paintGrid (juce::Graphics&) const;
	void paintPair (juce::Graphics&, const spatial::Pose&, juce::Colour, bool filled) const;
	spatial::Pose dragged (juce::Point<float>) const;

	spatial::Pose home, now;
	motionlite::PadState state = motionlite::PadState::idle;
	Grab grab = Grab::none;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StemViewer)
};
