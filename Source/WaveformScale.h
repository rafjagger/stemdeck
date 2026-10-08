#pragma once

#include <cmath>

// How the scrolling waveform maps wall-clock time to track time.
// The window is a fixed number of wall-clock seconds; the stretch of track it
// shows grows with the deck's playback rate, so synced decks of different
// native tempos draw their beats equally far apart.
namespace waveformScale
{
	// A stopped, reversed or scratched deck has no usable rate; draw it at 1.
	inline double sanitisedRate (double rate)
	{
		return std::isfinite (rate) && rate > 0.0 ? rate : 1.0;
	}

	// Seconds of track visible in a window of `visibleSeconds` wall-clock seconds.
	inline double trackSpan (double visibleSeconds, double rate)
	{
		return visibleSeconds * sanitisedRate (rate);
	}

	inline double pixelsPerTrackSecond (double widthPixels, double visibleSeconds, double rate)
	{
		return widthPixels / trackSpan (visibleSeconds, rate);
	}
}
