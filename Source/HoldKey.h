// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

#include <set>
#include <utility>

// A key that reports being held and let go, counted by the pointers on it.
//
// juce::Button keeps one state for every pointer: when a second finger lands
// elsewhere -- PLAY, while CUE is held -- it leaves its down state, and a key
// read from isDown() is let go under a finger that never moved (2026-10-28).
// Here each pointer is its own (source type, index): every touch reaches JUCE
// twice, as touch#0 and X's emulated mouse#0, so the index alone is not
// enough (feedback_every-touch-arrives-twice). Held from the first pointer
// down to the last one up.
class HoldKey : public juce::TextButton
{
public:
	using juce::TextButton::TextButton;

	std::function<void()> onHold, onLetGo;

	bool isHeld() const { return ! pointers.empty(); }

	void mouseDown (const juce::MouseEvent& e) override
	{
		juce::TextButton::mouseDown (e);
		const auto wasHeld = isHeld();
		pointers.insert (pointerOf (e));
		if (! wasHeld && onHold)
			onHold();
	}

	void mouseUp (const juce::MouseEvent& e) override
	{
		juce::TextButton::mouseUp (e);
		if (pointers.erase (pointerOf (e)) == 0 || isHeld())
			return;
		if (onLetGo)
			onLetGo();
	}

private:
	using Pointer = std::pair<int, int>;
	static Pointer pointerOf (const juce::MouseEvent& e) { return { (int) e.source.getType(), e.source.getIndex() }; }

	std::set<Pointer> pointers;
};
