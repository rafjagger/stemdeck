// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "MotionLite.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

class DeckMotion;

// One stem's one-shot keys, as both screens show them: ORBIT, PING, RISE,
// SCAT (idle plain, armed blinking on the deck's beat, running in the stem's
// colour with its beat count), the length keys and the length between them.
// The keys are children of whatever adds them and placed by it: the two-deck
// call-out lays them in a row, the one-deck stem column two by two. A press
// asks `takeShift` whether it starts now (SHIFT; the key lets go after one
// press) and goes through the deck's motion queue.
class OneShotKeys
{
public:
	OneShotKeys (DeckMotion&, int stem, std::function<bool()> takeShift);

	void addTo (juce::Component& parent);
	void setStemColour (juce::Colour);
	// Reads what the audio thread last did into the faces and the length.
	void refresh();

	juce::TextButton& pad (motionlite::Shape shape) { return pads[(size_t) shape]; }
	juce::TextButton& lengthDown() { return shorter; }
	juce::TextButton& lengthUp() { return longer; }
	juce::Label& length() { return lengthLabel; }

	static juce::String shapeName (motionlite::Shape);
	static juce::String lengthName (double bars);   // "½ BAR", "1 BAR", "4 BARS"
	// The stem's state in words: "ORBIT 4 · 2/4" running, "PING 2 · armed", "home".
	static juce::String stateLine (const DeckMotion&, int stem);

private:
	void press (motionlite::Shape);
	void lookOfPad (motionlite::Shape);

	DeckMotion& motion;
	const int stem;
	std::function<bool()> takeShift;
	juce::Colour stemColour;

	std::array<juce::TextButton, 4> pads;
	juce::TextButton shorter, longer;
	juce::Label lengthLabel;

	JUCE_DECLARE_NON_COPYABLE (OneShotKeys)
};
