#pragma once

// How the transport CUE key lights, like the CUE button of a Pioneer CDJ.
// Pure: no JUCE.
namespace cuelight
{
	enum class CueLight { off, steady, blink };

	// The playhead counts as on the cue point within 10 ms: more than one audio
	// block (256 frames at 48 kHz are 5.3 ms), less than a 60 Hz UI frame
	// (16.7 ms), so a jump back to the cue always reads as "on".
	constexpr double onCueTolerance = 0.010;

	// 2 Hz: 250 ms on, 250 ms off.
	constexpr double blinkHalfPeriod = 0.25;

	CueLight cueLight (bool loaded, bool playing, bool held, double position, double cuePoint);
	bool blinkPhaseOn (double seconds);
}
