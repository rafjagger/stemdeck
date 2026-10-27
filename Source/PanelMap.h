#pragma once

#include "Buses.h"
#include "PanelEdges.h"
#include "PanelProtocol.h"

#include <array>
#include <optional>
#include <vector>

// What the A³ Motion panel means to StemDeck, and what it shows. Pure: no
// JUCE, testable.
//
// The panel splits down the middle, deck A on the left half, B on the right,
// each half five columns wide. A stem's row reads like the screen's, column
// for column:
//
//   row 0     CUE   (pots / encoders over the pads)               CUE
//   row 1     PLAY                                                PLAY
//   rows 2-5  .  M  C  AUX  n   |   n  AUX  C  M  .      a row per stem, 1-4
//
// n is the stem's own channel, AUX the return, C its cue and M its mute; the
// dot is the dark column where the screen row has its knob -- no control,
// no light. A pad switches its stem onto that bus or off it, through the
// mixer's own switch (MixerPanel::pressBus), toggles its cue, or its mute
// (what the encoder's push does) -- what a click on the screen key does.
// A stem's pads light as the screen keys of its row do: the channel pad in the
// stem's colour while on, the AUX pad in AUX's, the cue pad in the cue colour
// while the stem is cued, the mute pad in the mute colour while it is muted,
// and the screen key's own dim grey when off. Only CUE and PLAY rest in the
// deck's colour, so the halves still read as A and B, and light in theirs. Over each half, four encoders -- two columns of two -- turn the
// four stems' gains in reading order; a push mutes the stem. The inner pot of
// each half is the deck's fader; the outer pots are left free.
namespace panel
{
	struct Control
	{
		enum class Kind { none, route, cue, play, stemCue, stemMute };
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
		enum class Type { route, cueDown, cueUp, play, stemCue, gain, mute, fader };
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
		EdgeTracker tracker;
		std::array<std::optional<int>, potCount> potAt {};
	};

	//==============================================================================
	using Leds = std::array<Rgb, ledCount>;

	// A UI colour as an LED should show it: the hue kept, the saturation
	// tripled (capped), at full brightness. The stems' pastels otherwise wash
	// out to nearly white on an RGB LED. Black stays black.
	Rgb vivid (Rgb colour);

	struct LedState
	{
		buses::Masks masks {};
		std::array<bool, buses::stemCount> muted {}, cued {};
		std::array<Rgb, buses::stemsPerDeck> stemColours {};
		std::array<bool, buses::decks> playing {}, atCue {};
		Rgb playColour, cueColour, muteColour, auxColour;
		Rgb offColour;   // a stem pad that is off: the screen key's own, shown as it is
		std::array<Rgb, buses::decks> deckColours {};   // every key's resting colour, by half
	};

	// `c` at numerator/denominator of its brightness.
	Rgb scaled (Rgb c, int numerator, int denominator);

	// A key's resting light in `colour` (its deck's): vivid, the brightest
	// channel at Motion's idle level (214).
	Rgb base (Rgb colour);

	// Every LED as wanted, by its id in the chain. Sent as it is: the current
	// limit is the panel firmware's (LED_BUDGET_MA, all LEDs scaled evenly
	// above it), the same fuse A3 Motion runs on.
	Leds compose (const LedState& state);
}
