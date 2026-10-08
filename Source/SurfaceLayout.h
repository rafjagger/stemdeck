#pragma once

#include "Buses.h"

#include <array>

// Where everything on the main surface sits (the "clean surface", 2026-10-08).
// Pure: no JUCE, so the geometry can be tested at the rig's 768 x 1024 without
// a window. The components call these with their own bounds and apply the
// result (SurfaceJuce.h converts); MainComponent places the sections.
//
// Sizes are shares of the container they hang on, not pixels: a key row is a
// share of the deck's height, a switch a share of the stem row's width.
namespace surface
{
	struct Rect
	{
		int x = 0, y = 0, w = 0, h = 0;

		int right() const { return x + w; }
		int bottom() const { return y + h; }
		bool isEmpty() const { return w <= 0 || h <= 0; }
		bool contains (const Rect& other) const;
		bool intersects (const Rect& other) const;   // sharing an edge is not overlapping
		Rect translated (int dx, int dy) const { return { x + dx, y + dy, w, h }; }
		Rect reduced (int dx, int dy) const;
		Rect reduced (int d) const { return reduced (d, d); }
		Rect withSizeKeepingCentre (int width, int height) const;

		// Cut a slice off one side and return it; clamped to what is there.
		Rect removeFromTop (int amount);
		Rect removeFromBottom (int amount);
		Rect removeFromLeft (int amount);
		Rect removeFromRight (int amount);

		bool operator== (const Rect& o) const { return x == o.x && y == o.y && w == o.w && h == o.h; }
		bool operator!= (const Rect& o) const { return ! (*this == o); }
	};

	// The window: top bar; the waveform section with a pitch column at each
	// outer edge (A left, B right); deck A | mixer | deck B; the library.
	struct Sections
	{
		Rect topBar, waves, waveA, waveB;
		std::array<Rect, 2> pitch, deck;
		Rect mixer, library;
	};
	Sections sections (int width, int height);

	// A pitch column, in its own coordinates: the fader with its value box
	// below it (the slider's text box, `valueHeight` tall), the range key under that.
	struct PitchColumn
	{
		Rect fader, range;
		int valueHeight = 0;
	};
	PitchColumn pitchColumn (Rect local);

	// A deck column, in its own coordinates. With GRID on, the Grid Adjust
	// rows take their room from the overview: no key moves.
	struct DeckColumn
	{
		Rect title, stems, overview, elapsed, remaining, bpm, bpmInfo;
		Rect previous, cue, play, next;
		Rect loopOff, repeat, sync, master, vinyl, grid;
		std::array<Rect, 5> gridNudge;   // <1/2  <1  SET 1  1>  1/2>
		std::array<Rect, 3> gridEdit;    // SNAP  SHIFT  RESET
	};
	DeckColumn deckColumn (Rect local, bool gridOn);

	// One stem's row in a channel strip: the knob, the name above one row of
	// the bus switches 1 2 3 4 A and the mute.
	struct StemRow
	{
		Rect frame, knob, label, mute;
		std::array<Rect, buses::count> buses;
	};

	// A channel strip, in its own coordinates: header, stem rows, and the
	// volume fader directly beside the meter zone (towards the mixer's middle:
	// deck A's right, deck B's left), its VU drawn in its slot.
	struct Strip
	{
		Rect header;
		std::array<StemRow, buses::stemsPerDeck> stems;
		Rect fader, meterZone;
	};
	Strip channelStrip (Rect local, int deckIndex, int meterReserve);

	// The mixer, in its own coordinates: two strips meeting in the middle, the
	// output meters over their inner corners, as tall as the faders' zone.
	struct Mixer
	{
		std::array<Rect, 2> strip;
		Rect meters;
		int meterReserve = 0;
	};
	Mixer mixer (Rect local);

	// The output meters' caption line (bus names, L/R) under the bars; the
	// faders end where the bars do.
	int meterCaptionHeight (int metersHeight);
}
