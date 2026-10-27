// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// The setups every knob and key of the one-deck screen shares, so each is
// decided once.
namespace knobsetup
{
	// A rotary knob on `min`..`max`: dragged either way, no text box, a value
	// popup while turned, a double click back to `fresh` (which it starts at).
	// It never takes the keyboard focus from a click.
	inline void rotaryKnob (juce::Slider& knob, double min, double max, double step, double fresh)
	{
		knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
		knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
		knob.setRange (min, max, step);
		knob.setValue (fresh, juce::dontSendNotification);
		knob.setDoubleClickReturnValue (true, fresh);
		knob.setPopupDisplayEnabled (true, true, nullptr);
		knob.setMouseClickGrabsKeyboardFocus (false);
	}

	// A key the touch screen presses and the keyboard (Tab, space) never reaches.
	inline void untabbableKey (juce::Button& key)
	{
		key.setMouseClickGrabsKeyboardFocus (false);
		key.setWantsKeyboardFocus (false);
	}

	// Turns a knob by encoder detents, 1/48 of its travel each: some two turns
	// from one end to the other.
	inline void nudge (juce::Slider& knob, double detents)
	{
		constexpr double detentShare = 1.0 / 48.0;
		const auto travel = knob.valueToProportionOfLength (knob.getValue()) + detents * detentShare;
		knob.setValue (knob.proportionOfLengthToValue (juce::jlimit (0.0, 1.0, travel)), juce::sendNotificationSync);
	}
}
