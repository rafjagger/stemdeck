#pragma once

#include "StemHandover.h"

#include <array>
#include <optional>

// The Auto-DJ: plays track after track on the two decks and mixes each into
// the next, beat-matched, as a DJ would. Pure: no JUCE, testable -- it sees
// the decks, says what to do, and MainComponent does it.
//
//   - Nothing playing when it goes on: a track onto deck A, from the start.
//   - Half a minute before the mix: the next track onto the other deck.
//   - It mixes on the desk's buses: bus n carries stem n -- drums 1, bass 2,
//     other 3, vocals 4 -- of one deck at a time, and a stem changes over by
//     its bus switching source on a downbeat, from the old deck's stem to the
//     new one's. A stem not playing is on no bus, never on AUX. The faders
//     stay at unity, nothing fades, and the mute buttons are the DJ's alone.
//   - Taking over (switched on with a deck playing, or its first track
//     starting): the playing deck's stems onto buses 1-4, the other deck's
//     onto none.
//   - Both tracks on a beat grid: they overlap for about 20 s, in whole bars
//     of the playing track at its tempo (at least 4; a bar count can be set
//     instead), so that the overlap ends with the playing track. The new
//     track starts on a downbeat from its own first downbeat, SYNC on, and
//     its drums take bus 1 on that downbeat. Bass, then other, change over
//     one at a time, where the stems' envelopes say it is least heard
//     (StemHandover.h). The old vocals sing to the end of the old track; the
//     new ones take bus 4 on the first downbeat after it, and SYNC comes off.
//   - Without a grid on either: four steps over a time in seconds (10 s by
//     default), unsynced -- drums at the start, bass a third in, other two
//     thirds in, vocals at the end as the old deck stops.
//   - The old deck stops on no bus. Turning AutoDJ off leaves the routing as
//     it is, the DJ's to carry on from.
//   - A track is where it is heard (StemHandover::audibleSpan), once its
//     levels are read: the old one ends at its audible end -- the vocals
//     change over, and the deck stops, on the downbeat closing the bar it
//     ends in -- and the load ahead counts from there. The new one starts on
//     the first downbeat of its sound: the downbeat it starts on (within a
//     tenth of a second), the one before when it starts in a bar's last beat
//     (a pickup, heard whole), else the next one. Without a grid, at its
//     audible start. Until the levels are there, the file's start and end;
//     a mix waits up to a few seconds for the new track's.
// Next / Prev on the playing deck mixes now: the track beside it onto the
// other deck and the same handover from the next downbeat, not waiting for
// the end; the vocals change over and the old deck stops at the overlap's end.
// A loop is the DJ's: nothing is loaded onto a deck that loops, and no mix
// starts while the playing deck loops -- it waits until the loop is off.
class AutoDj
{
public:
	static constexpr int numStems = StemHandover::numStems;

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
		std::optional<double> audibleStart, audibleEnd;   // track seconds, once known
		bool levelsPending = false;                       // being read
		const StemHandover::Envelopes* levels = nullptr;   // the loaded track's, once analysed
	};

	// Which track a load means: AutoDJ's own random pick, or the one beside
	// the playing track (Next / Prev), as the library lists them.
	enum class Pick { random, next, previous };

	// Where a stem goes: bus 0-3 (its own number), or none.
	static constexpr int offBus = -1;

	// What to do this tick; -1 / empty: nothing. The routing goes before the
	// start, so the new track starts with only the stems the handover lets in.
	struct Commands
	{
		int load = -1;               // a new track onto this deck
		Pick loadPick = Pick::random;
		int start = -1;              // play this deck from `startAt`
		double startAt = 0.0;        // track seconds
		int stop = -1;
		int syncOn = -1, syncOff = -1;
		std::array<std::optional<double>, 2> faderDb;
		std::array<std::array<std::optional<int>, numStems>, 2> bus;   // per deck and stem
	};

	// How long a mix runs: bars on the playing track's grid (or about 20 s
	// of them), or seconds without one. A new length applies to the next
	// mix, not a running one.
	struct MixLength
	{
		int bars;
		double noGridSeconds;
	};
	static constexpr int aboutTwentySeconds = 0;
	static constexpr int defaultMixBars = aboutTwentySeconds;
	static constexpr int minOverlapBars = 4;
	static constexpr double overlapSeconds = 20.0;
	static constexpr double defaultNoGridMixSeconds = 10.0;
	static constexpr double loadAheadSeconds = 30.0;
	static constexpr double maxLevelsWait = 6.0;    // seconds a mix waits for the new track's levels
	static constexpr double onTheDownbeat = 0.1;    // a sound this soon after a downbeat starts on it

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

private:
	// A running mix, fixed when it starts.
	struct Mix
	{
		bool synced = false;
		bool toTrackEnd = true;
		// Synced: downbeats of the new track from its start.
		int bassBar = 0, otherBar = 0, endBar = 0;
		// Unsynced: the old track's seconds.
		double start = 0.0, length = 0.0;
		std::array<bool, numStems> handedOver {};
		double incomingStart = 0.0;   // where the new track started, a downbeat on its grid
	};

	static int overlapBars (const DeckView& deck, MixLength length);
	static double audibleEnd (const DeckView& deck);
	static double startPosition (const DeckView& deck);
	// The mix in the playing track's own seconds.
	static double mixSeconds (const DeckView& deck, bool synced, MixLength length);
	bool takeRequest (Commands& out);
	void startMix (Commands& out, const std::array<DeckView, 2>& decks);
	void runSyncedMix (Commands& out, const std::array<DeckView, 2>& decks);
	void runUnsyncedMix (Commands& out, const std::array<DeckView, 2>& decks);
	void finishMix (Commands& out);

	void takeOver (Commands& out, int deck);
	void handOver (Commands& out, int stem);

	bool enabled = false;
	Phase state = Phase::off;
	int current = 0;
	double lastBarPosition = -1.0;
	double progress = 0.0;
	MixLength setting { defaultMixBars, defaultNoGridMixSeconds };
	MixLength running = setting;     // latched when a mix starts
	std::optional<Pick> request;
	bool mixNow = false;             // the waiting track mixes at the next downbeat
	bool otherLoops = false;         // seen at the last update
	Mix mix;
	double lastIncomingPosition = 0.0;
	double levelsWaitFrom = -1.0;     // the playing track's position when the wait began
};
