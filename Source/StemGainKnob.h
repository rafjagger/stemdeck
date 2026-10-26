// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include "StemKnob.h"

// A stem's GAIN knob, the same on the mixer's strip and on the one-deck
// screen's stem column: dB from stemKnobMinimumDb (silence, R4) to +6, the
// travel's middle at -12, a double click back to 0 dB.
namespace stemgain
{
	inline juce::String dbText (double db)
	{
		return db <= stemKnobMinimumDb ? juce::String ("-inf dB") : juce::String (db, 1) + " dB";
	}

	// The knob's dB as the player's linear gain: exactly 0 at the bottom.
	inline float gainOf (double db)
	{
		return juce::Decibels::decibelsToGain ((float) db, stemKnobMinimumDb);
	}

	inline void setUp (juce::Slider& knob)
	{
		knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
		knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
		knob.setRange (stemKnobMinimumDb, 6.0, 0.1);
		knob.setSkewFactorFromMidPoint (-12.0);
		knob.setValue (0.0, juce::dontSendNotification);
		knob.setDoubleClickReturnValue (true, 0.0);
		knob.setPopupDisplayEnabled (true, true, nullptr);
		knob.textFromValueFunction = dbText;
		knob.setMouseClickGrabsKeyboardFocus (false);
	}
}
