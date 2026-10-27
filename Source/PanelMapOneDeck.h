// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "MotionLite.h"
#include "PanelMap.h"

#include <array>
#include <vector>

// What the A³ Motion panel means to a one-deck (-bformat) StemDeck, and what
// it shows (spec section 10, B16): the stems stand under their encoders. Pure:
// no JUCE, testable.
//
//   col 0:  row 0 CUE, row 1 PLAY, rows 2-5 dark
//   col 9:  row 0 SHIFT (held), rows 1-5 dark
//   stem s (0-3): pad columns 2s+1 and 2s+2
//       row 2  ORBIT  PING
//       row 3  RISE   SCAT
//       row 4  CUE    MUTE
//       row 5  <      >      (the one-shot length)
//   stem s's encoders: the upper one is GAIN (its push mutes), the lower is 3D.
//   The pots are unused.
namespace panel1
{
	using panel::Cell;
	using panel::Rgb;

	struct Control
	{
		enum class Kind { none, cue, play, shift, orbit, ping, rise, scat, stemCue, stemMute, lengthDown, lengthUp };
		Kind kind = Kind::none;
		int stem = -1;
	};

	Control controlAt (Cell cell);
	// Where a control's key is; {-1, -1} for none.
	Cell cellOf (const Control& control);

	struct StemKnob { int stem = -1; bool threeD = false; };
	// encoder % 4 is the stem; the upper four (>= 4) turn GAIN, the lower 3D.
	StemKnob encoderStem (int encoder);

	struct Event
	{
		enum class Type { cueDown, cueUp, play, shiftDown, shiftUp, shot, stemCue, stemMute, lengthDown, lengthUp, gain, threeD };
		Type type = Type::play;
		int stem = -1;
		motionlite::Shape shape = motionlite::Shape::orbit;   // shot
		double value = 0.0;                                   // gain, threeD: encoder detents, + clockwise
	};

	// Turns polled frames into events: a key reports its press (CUE and SHIFT
	// their release too) once, not on every poll it is held.
	class InputDecoder
	{
	public:
		std::vector<Event> feed (const panel::Frame& frame);
		void reset() { tracker.reset(); }

	private:
		panel::EdgeTracker tracker;
	};

	//==============================================================================
	struct StemLeds
	{
		std::array<motionlite::PadState, 4> pads {};   // by motionlite::Shape
		bool cued = false, muted = false;
		bool shortest = false, longest = false;         // the one-shot length is at an end of its range
	};

	// Everything the panel shows, as plain values: the screen's state is read
	// into this by the owner, never passed in as a component.
	struct LedState
	{
		std::array<StemLeds, 4> stems {};
		std::array<Rgb, 4> stemColours {};
		Rgb deckColour, cueColour, playColour, muteColour;
		bool playing = false, atCue = false, shift = false;
	};

	// Every LED as wanted. Armed pads blink with a period of 500 ms.
	panel::Leds leds (const LedState& state, double nowSeconds);
}
