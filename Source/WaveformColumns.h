#pragma once

#include <algorithm>
#include <cmath>

// The scrolling waveform's pixel columns, anchored to the track.
//
// Column k always covers the track from k * secondsPerColumn to
// (k + 1) * secondsPerColumn, whatever the playhead does, and the view moves
// by whole columns. A peak therefore keeps its column's min/max while it
// scrolls past. Resampling from a fractional window start every frame (what
// juce::AudioThumbnail::drawChannel does) gave each column different samples
// every frame, and the peaks sparkled (2026-10-16).
namespace waveformColumns
{
	// Column widths are whole multiples of this, so a change of span far below
	// a pixel (a sync nudge, a fader step) leaves the boundaries where they are.
	constexpr double quantaPerSecond = 96000.0;

	struct Grid
	{
		double secondsPerColumn;
		long long originColumn;   // the column drawn at x = 0
	};

	inline double secondsPerColumn (double spanSeconds, int widthPixels)
	{
		const auto raw = spanSeconds / (double) std::max (1, widthPixels);
		const auto quanta = std::isfinite (raw) ? std::round (raw * quantaPerSecond) : 1.0;
		return std::max (1.0, quanta) / quantaPerSecond;
	}

	// The grid that puts `position` within half a pixel of `playheadX`.
	inline Grid gridFor (double position, double spanSeconds, int widthPixels, double playheadX)
	{
		const auto spc = secondsPerColumn (spanSeconds, widthPixels);
		return { spc, (long long) std::llround (position / spc - playheadX) };
	}

	inline double columnStart (long long k, const Grid& g) { return (double) k * g.secondsPerColumn; }
	inline double columnEnd (long long k, const Grid& g)   { return columnStart (k + 1, g); }
	inline long long columnAt (double t, const Grid& g)    { return (long long) std::floor (t / g.secondsPerColumn); }
	inline double columnX (long long k, const Grid& g)     { return (double) (k - g.originColumn); }

	// For everything drawn over the columns (beats, loop, cue), on their scale.
	inline double timeToX (double t, const Grid& g)        { return t / g.secondsPerColumn - (double) g.originColumn; }
}
