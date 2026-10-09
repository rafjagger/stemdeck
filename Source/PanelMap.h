#pragma once

#include "Buses.h"
#include "PanelProtocol.h"

#include <array>
#include <optional>
#include <vector>

// What the A³ Motion panel means to StemDeck, and what it shows. Pure: no
// JUCE, testable.
//
// The panel splits down the middle, deck A on the left half, B on the right,
// each half five columns wide -- the deck's routing matrix as the screen
// shows it, in the same order (buses::columnOrder):
//
//   row 0     CUE   (pots / encoders over the pads)               CUE
//   row 1     PLAY                                                PLAY
//   rows 2-5  AUX  1  2  3  4   |   1  2  3  4  AUX     a row per stem, 1-4
//
// A pad switches its stem onto that bus or off it, through the mixer's own
// switch (MixerPanel::pressBus) -- what a click on the screen key does. Every
// key glows at rest in what it is (the stem's colour on its row, CUE and PLAY
// in theirs) and lights at full over that glow while it is on: the stem on
// that bus, a quarter while the stem is muted. Over each half, four encoders -- two columns of two -- turn
// the four stems' gains in reading order; a push mutes the stem. The inner
// pot of each half is the deck's fader; the outer pots are left free.
namespace panel
{
	struct Control
	{
		enum class Kind { none, route, cue, play };
		Kind kind = Kind::none;
		int deck = -1, stem = -1, bus = -1;
	};

	Control controlAt (Cell cell);
	// Where a control's key is; {-1, -1} for none.
	Cell cellOf (const Control& control);

	// The encoder that turns a stem's gain, and the pot that is a deck's fader.
	struct StemKnob { int deck = -1, stem = -1; };
	StemKnob encoderStem (int encoder);
	int faderPot (int deck);

	struct Event
	{
		enum class Type { route, cueDown, cueUp, play, gain, mute, fader };
		Type type = Type::route;
		int deck = -1, stem = -1, bus = -1;
		double value = 0.0;   // gain: encoder detents, + clockwise; fader: 0..1 of its travel
	};

	// Turns polled frames into events: a key reports its press (and CUE its
	// release) once, not on every poll it is held.
	class InputDecoder
	{
	public:
		std::vector<Event> feed (const Frame& frame);

		// A panel (re)connected: every key up, every pot's position unknown.
		void reset();

		// ADC counts a pot must move before it counts: below it is noise.
		static constexpr int potDeadband = 8;

	private:
		std::array<bool, buttonCount> keyDown {};
		std::array<bool, encoderCount> pushDown {};
		std::array<std::optional<int>, potCount> potAt {};
	};

	//==============================================================================
	using Leds = std::array<Rgb, ledCount>;

	// The LED budget: the sum of every LED's r + g + b never above eight LEDs
	// at full white -- what the panel's supply carries with its other loads,
	// about 370 mA.
	constexpr int budget = 8 * 3 * 255;

	// A UI colour as an LED should show it: the hue kept, the saturation
	// tripled (capped), at full brightness. The stems' pastels otherwise wash
	// out to nearly white on an RGB LED. Black stays black.
	Rgb vivid (Rgb colour);

	// Every LED scaled down by one factor until the total is within budget;
	// unchanged when it already is.
	Leds withinBudget (const Leds& leds);

	struct LedState
	{
		buses::Masks masks {};
		std::array<bool, buses::stemCount> muted {};
		std::array<Rgb, buses::stemsPerDeck> stemColours {};
		std::array<bool, buses::decks> playing {}, atCue {};
		Rgb playColour, cueColour;
	};

	// A key's resting glow in `colour`: vivid, at a twelfth.
	Rgb base (Rgb colour);

	// Every LED as wanted, by its id in the chain, before the budget.
	Leds compose (const LedState& state);
	// The same within budget: what the panel is sent.
	Leds render (const LedState& state);
}
