// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include "MeterBallistics.h"

#include <optional>

// Vertical peak meter fed from a player's stem peaks, with the system's
// ballistics (PeakMeter) and a line where the held peak stands.
// With a clip lamp, its top segment lights only above full scale and holds.
class LevelMeter : public juce::Component
{
public:
	void setLevel (float newPeak);
	void setParameters (MeterParameters parameters) { ballistics.setParameters (parameters); }
	void setShowsClip (bool shows) { showsClip = shows; repaint(); }
	// The bar's normal range in this colour (a stem's) instead of the LEDs' green.
	void setBarColour (juce::Colour colour) { barColour = colour; repaint(); }
	void paint (juce::Graphics& g) override;

private:
	float secondsSinceLastFeed();

	PeakMeter ballistics;
	double lastFeedMs = 0.0;
	int litSegments = 0, heldSegment = 0;
	bool showsClip = false;
	std::optional<juce::Colour> barColour;
	ClipHold clipHold;
	bool clipping = false;
};
