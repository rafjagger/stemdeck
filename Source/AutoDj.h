#pragma once

#include <array>
#include <optional>

// The Auto-DJ: plays track after track on the two decks and mixes each into
// the next, beat-matched, as a DJ would. Pure: no JUCE, testable -- it sees
// the decks, says what to do, and MainComponent does it.
//
//   - Nothing playing when it goes on: a track onto deck A, from the start.
//   - Half a minute before the mix: the next track onto the other deck, its
//     fader down.
//   - The mix starts on a downbeat of the playing track, so that it ends with
//     the track (16 bars before its end by default; the length is a setting): the new track from its
//     own first downbeat, SYNC on it (tempo, beats and bars follow), and over
//     those 16 bars the faders cross, equal power.
//   - Then the old deck stops, SYNC comes off, and the new one is the one
//     playing. Its tempo stays where the mix put it.
// Tracks without a beat grid mix over a fixed time (10 s by default), unsynced.
// Next / Prev on the playing deck mixes now: the track beside it onto the
// other deck and the mix from the next downbeat, not waiting for the end.
// A loop is the DJ's: nothing is loaded onto a deck that loops, and no mix
// starts while the playing deck loops -- it waits until the loop is off.
class AutoDj
{
public:
	struct DeckView
	{
		bool loaded = false;
		bool playing = false;
		double position = 0.0;   // track seconds
		double length = 0.0;
		double gridBpm = 0.0;    // 0: no grid
		double firstBeat = 0.0;
		double rate = 1.0;       // track seconds per second (tempo fader and sync)
		bool looping = false;
	};

	// Which track a load means: AutoDJ's own random pick, or the one beside
	// the playing track (Next / Prev), as the library lists them.
	enum class Pick { random, next, previous };

	// What to do this tick; -1 / empty: nothing.
	struct Commands
	{
		int load = -1;               // a new track onto this deck
		Pick loadPick = Pick::random;
		int start = -1;              // play this deck from its first downbeat (or the start)
		int stop = -1;
		int syncOn = -1, syncOff = -1;
		std::array<std::optional<double>, 2> faderDb;
	};

	// How long a mix runs: bars on the playing track's grid, or seconds
	// without one. A new length applies to the next mix, not a running one.
	struct MixLength
	{
		int bars;
		double noGridSeconds;
	};
	static constexpr int defaultMixBars = 16;
	static constexpr double defaultNoGridMixSeconds = 10.0;
	static constexpr double loadAheadSeconds = 30.0;
	static constexpr double silentDb = -60.0;   // the faders' bottom

	void setEnabled (bool on);
	bool isEnabled() const { return enabled; }

	void setMixLength (MixLength length);
	MixLength mixLength() const { return setting; }

	Commands update (const std::array<DeckView, 2>& decks);

	// Next / Prev on the playing deck: the mix to that track starts now, on
	// the next downbeat. Only while a track plays alone -- not while it
	// starts, not during a mix (false: ignored).
	bool canMixNow() const;
	bool requestMixNow (Pick target);

	// Where it is, for the status line.
	enum class Phase { off, starting, playing, preparing, ready, mixing };
	Phase phase() const { return state; }
	int playingDeck() const { return current; }
	double mixProgress() const { return progress; }

	// The equal-power fader pair at `progress` 0..1 of a mix, in dB.
	static double fadeInDb (double progress);
	static double fadeOutDb (double progress);

private:
	// The mix in the playing track's own seconds: from `mixStart`, this long.
	static double mixSeconds (const DeckView& deck, MixLength length);
	bool takeRequest (Commands& out);

	bool enabled = false;
	Phase state = Phase::off;
	int current = 0;
	double mixStart = 0.0;
	double lastBarPosition = -1.0;
	double progress = 0.0;
	MixLength setting { defaultMixBars, defaultNoGridMixSeconds };
	MixLength running = setting;     // latched when a mix starts
	std::optional<Pick> request;
	bool mixNow = false;             // the waiting track mixes at the next downbeat
	bool otherLoops = false;         // seen at the last update
};
