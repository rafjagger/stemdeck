#pragma once

#include "Buses.h"

#include <array>

// Where everything on the main surface sits (the "clean surface", 2026-10-08).
// Pure: no JUCE, so the geometry can be tested at the rig's 768 x 1024 without
// a window. The components call these with their own bounds and apply the
// result (SurfaceJuce.h converts); MainComponent places the sections and the
// band, whose parts belong to the decks and the mixer.
//
// Sizes are shares of what they hang on, not pixels: the deck's rows are
// shares of a key, the key a share of the window's height.
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

	// The window: top bar; the two rolling waveforms, A over B, full width;
	// deck A | mixer | deck B; below them the band across the whole width;
	// the library.
	struct Sections
	{
		Rect topBar, waves, waveA, waveB;
		std::array<Rect, 2> deck;
		Rect mixer, band, library;
	};
	Sections sections (int width, int height);

	// The band under the decks and the mixer, in window coordinates. Per deck,
	// from the outside in: the pitch fader beside the overview and, under the
	// overview, the deck's title and stems line; under both the bottom line:
	// the pitch value, the range key and the BPM (big, and the original). Then
	// the volume fader (the stems' meters in its slot) inside the deck's colour
	// frame; the output meters in the middle. A on the left, B mirrored.
	struct BandDeck
	{
		Rect pitch, pitchValue, range, bpm, bpmInfo, title, stems, overview;
	};
	struct Band
	{
		std::array<BandDeck, 2> deck;
		std::array<Rect, 2> fader, faderFrame;
		Rect meters;
	};
	Band band (Rect area);

	// The deck's colour line, in window coordinates, as one piece: the stripe
	// at the deck column's inner edge (the column paints it, deckStripe), its
	// drop into the gap above the band, the top along that gap to the volume
	// fader's frame, and the frame's side towards the meters down to the
	// band's foot. Nothing below or outside the fader: the meters stand
	// between the two decks' lines.
	struct DeckLine
	{
		Rect stripe, drop, top, side;
	};
	Rect deckStripe (Rect deck, int deckIndex);   // in `deck`'s coordinates (window or local)
	DeckLine deckLine (const Sections& sections, const Band& band, int deckIndex);

	// A deck column above the band, in its own coordinates: the times, the
	// Grid Adjust rows, track search | CUE | PLAY | track search, the keys.
	// No key moves when GRID toggles: with GRID off the times take the Grid
	// Adjust rows' room (stacked and bigger), with GRID on they share a row.
	struct DeckColumn
	{
		Rect elapsed, remaining;
		Rect previous, cue, play, next;
		Rect loopOff, repeat, sync, master, vinyl, grid;
		std::array<Rect, 5> gridNudge;   // <1/2  <1  SET 1  1>  1/2>
		std::array<Rect, 3> gridEdit;    // SNAP  SHIFT  RESET
	};
	DeckColumn deckColumn (Rect local, bool gridOn);

	// How tall a deck column is for keys `keyHeight` high.
	int deckColumnHeight (int keyHeight);

	// One stem's row in a channel strip: the knob, the bus switches 1 2 3 4 A
	// and the mute. The rows lie tight under each other: a matrix of keys.
	struct StemRow
	{
		Rect frame, knob, mute;
		std::array<Rect, buses::count> buses;
	};

	// A channel strip above the band, in its own coordinates: the header and
	// the stem rows, which share its height.
	struct Strip
	{
		Rect header;
		std::array<StemRow, buses::stemsPerDeck> stems;
	};
	Strip channelStrip (Rect local);

	// The mixer above the band, in its own coordinates: two strips.
	std::array<Rect, 2> mixerStrips (Rect local);

	// The output meters' caption line (the bus names) under the bars; the
	// volume faders end where the bars do.
	int meterCaptionHeight (int metersHeight);
}
