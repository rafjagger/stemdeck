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
//     the track (16 bars before its end by default): the new track from its
//     own first downbeat, SYNC on it (tempo, beats and bars follow), and over
//     those 16 bars the faders cross, equal power.
//   - Then the old deck stops, SYNC comes off, and the new one is the one
//     playing. Its tempo stays where the mix put it.
// Tracks without a beat grid mix over a fixed 10 s, unsynced.
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
	};

	// What to do this tick; -1 / empty: nothing.
	struct Commands
	{
		int load = -1;               // a new track onto this deck
		int start = -1;              // play this deck from its first downbeat (or the start)
		int stop = -1;
		int syncOn = -1, syncOff = -1;
		std::array<std::optional<double>, 2> faderDb;
	};

	static constexpr int mixBars = 16;
	static constexpr double noGridMixSeconds = 10.0;
	static constexpr double loadAheadSeconds = 30.0;
	static constexpr double silentDb = -60.0;   // the faders' bottom

	void setEnabled (bool on);
	bool isEnabled() const { return enabled; }

	Commands update (const std::array<DeckView, 2>& decks);

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
	double mixLength (const DeckView& deck) const;

	bool enabled = false;
	Phase state = Phase::off;
	int current = 0;
	double mixStart = 0.0;
	double lastBarPosition = -1.0;
	double progress = 0.0;
};
