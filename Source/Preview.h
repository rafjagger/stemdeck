#pragma once

#include "Sections.h"

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// The preview StemDeck sends A3 Motion: where the
// audible deck is in its set's sections, and what comes next. Pure: no JUCE.
namespace preview
{
	// "none": no deck is audible, or the audible one is not analysed yet.
	// "end": no change before the set ends.
	constexpr const char* noneWord = "none";
	constexpr const char* endWord = "end";

	// The `stemdeck.ahead` word: s:section s:next i:barsUntilNext f:energy
	struct Ahead
	{
		std::string section = noneWord, next = noneWord;
		int barsUntilNext = 0;   // -1: a loop holds the change off
		float energy = 0.0f;

		bool operator== (const Ahead& other) const
		{
			// Exact on purpose: any change in what is said is news to the gate.
			return section == other.section && next == other.next && barsUntilNext == other.barsUntilNext
				&& std::equal_to<float>() (energy, other.energy);
		}
		bool operator!= (const Ahead& other) const { return ! (*this == other); }
	};

	// A loop, in bars since the downbeat.
	struct LoopBars
	{
		double start = 0.0, end = 0.0;
	};

	// At `barPosition` (bars since the downbeat, fractional). Before the
	// downbeat counts as bar 0; past the last bar is "none".
	Ahead aheadAt (const std::vector<sections::Bar>& bars, double barPosition, std::optional<LoopBars> loop);

	// A deck is audible while it plays with its fader above audibleFromGain.
	// Both audible: the master, if one of them is; else the louder fader,
	// A on a tie. None: -1.
	constexpr float audibleFromGain = 0.0316f;   // -30 dB

	struct DeckView
	{
		bool playing = false;
		float gain = 0.0f;
	};

	int audibleDeck (const std::array<DeckView, 2>& decks, int masterDeck);

	// When a preview goes out: on every downbeat (the bar changed) and on
	// any change -- another deck, another load, a jump, a loop, the pitch,
	// what the preview says. `generation` is the deck's load count.
	struct Moment
	{
		int deck = -1, generation = 0, bar = 0;
		long speedPermille = 1000;
		Ahead ahead;

		bool operator== (const Moment& other) const
		{
			return deck == other.deck && generation == other.generation && bar == other.bar
				&& speedPermille == other.speedPermille && ahead == other.ahead;
		}
	};

	// One deck as the timer sees it: what audibleDeck needs, and where the
	// deck is in its set.
	struct DeckState
	{
		DeckView view;
		int generation = 0;
		double speed = 1.0;                  // the tempo fader
		double bpm = 0.0, firstBeat = 0.0;   // the deck's grid; bpm 0: none yet
		double position = 0.0;               // seconds
		std::optional<std::pair<double, double>> loopSeconds;
		std::vector<sections::Bar> bars;     // empty: not analysed yet
	};

	// The moment of the deck that is heard: "none" when no deck is, or the
	// one heard has no grid or no sections yet.
	Moment momentOf (const std::array<DeckState, 2>& decks, int masterDeck);

	class Gate
	{
	public:
		// True when `now` differs from the moment last let through.
		bool shouldSend (const Moment& now);

	private:
		std::optional<Moment> last;
	};
}
