#pragma once

#include <array>
#include <functional>
#include <string>
#include <utility>
#include <vector>

// Which beat of a beat grid is the bar's one. Pure: no JUCE, testable.
//
// The tempo analysis finds the beats but not the bar: its first beat became
// the one, right about once in four tracks. Here each of the four beats of a
// bar is scored by what tends to land on a downbeat, from each stem on its
// own: drums and bass coming back after a break or stopping for one, chord
// changes, the arrangement changing at a bar line, bass notes and crashes.
// A silent or missing stem simply adds nothing.
namespace BarPhase
{
	constexpr int beatsPerBar = 4;   // 4/4 assumed

	// The stems the evidence comes from, by their place in a set.
	enum Stem { drums, bass, other, vocals, numStems };

	struct Stems
	{
		double sampleRate = 0.0;
		std::array<std::vector<float>, numStems> mono;   // an empty one is absent
	};

	using PerBeatOfBar = std::array<double, beatsPerBar>;

	// Whether the beats may move half a beat, onto the kick.
	enum class Beats { keep, ontoTheKick };

	struct Result
	{
		// The earliest downbeat at or after 0 s: within the first bar.
		double firstDownbeat = 0.0;
		bool movedHalfABeat = false;
		// Of the (moved) grid's beats, the earliest at or after 0 s is beat
		// 0; `offset` is the one that starts a bar (0 .. beatsPerBar - 1).
		int offset = 0;
		PerBeatOfBar scores {};
		// What each kind of evidence added to the scores, weighted.
		std::vector<std::pair<std::string, PerBeatOfBar>> evidence;
	};

	// `beatPhase`: the time of any beat, in seconds. Runs for a second or two
	// on a whole track; call it off the message thread.
	Result find (const Stems& stems, double bpm, double beatPhase, Beats beats,
				 const std::function<bool()>& shouldAbort = [] { return false; });

	// A cached grid's first beat found again: its tempo stays, its beats
	// move onto the kick if they were between, and the one is found anew.
	double foundAgain (const Stems& stems, double bpm, double firstBeat,
					   const std::function<bool()>& shouldAbort = [] { return false; });

	// The earliest downbeat at or after 0 s, when beat `offset` (counted from
	// the earliest beat at or after 0 s) is a one: within the first bar.
	double firstDownbeat (double bpm, double beatPhase, int offset);
}
