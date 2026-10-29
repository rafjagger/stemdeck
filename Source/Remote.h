#pragma once

#include "Buses.h"

#include <array>
#include <atomic>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// StemDeck by remote control from the A3 Mixer (spec stemdeck-remote,
// 2026-10-01): Core sets a stem's bus switch, StemDeck reports the stem's
// switches back as a bit mask, and sends one level meter per stem to the
// desk. The address patterns come from the one truth (a3-osc.json), read by
// OscTruthFile; this part only fills and takes them apart. Pure: no JUCE.
namespace remote
{
	// The truth's patterns, e.g. "/stemdeck/{deck}/{stem}/bus/{bus}".
	struct Words
	{
		std::string bus, buses, recall, vu, hello;
		// The truth's vu_meters: meter n is meters[n - 1]. Empty in an older truth.
		std::vector<std::string> meters;
		// The preview of the music, the `stemdeck.ahead` word. Optional: empty
		// in a truth from before it, and then StemDeck sends no preview.
		std::string ahead;
		// The pro-DJ pointer and the pro-DJ LIVE switch (`stemdeck.pointer`,
		// `prodj.live`). Optional likewise: empty in a truth from before them,
		// and then StemDeck listens to neither and shows no badge.
		std::string pointer, live;
		// A one-deck instance's words (`stemdeck.3d`, `stemdeck.deck-hello`).
		// Optional: empty in a truth from before them, and then a one-deck
		// StemDeck sends no deck hello and takes no 3D.
		std::string deckThreeD, deckHello;
		// `stemdeck.tempo`: the deck's tempo report. Optional likewise: empty
		// in a truth from before it, and then nothing is sent.
		std::string deckTempo;
		// The truth's {deck} range of `stemdeck.deck-hello`: the decks Core
		// hears. None in a truth without it.
		std::optional<std::pair<int, int>> deckRange;
	};

	// One switch, 0-based inside StemDeck: deck 0-1, stem 0-3, bus 0-4 (1-4, AUX).
	struct Switch
	{
		int deck = 0, stem = 0, bus = 0;
		bool on = false;
	};

	std::optional<Switch> parseSwitch (const Words& words, const std::string& address, int value);
	std::string reportAddress (const Words& words, int deck, int stem);
	bool isRecall (const Words& words, const std::string& address);
	std::string vuAddress (const Words& words, int deck, int stem);
	// One-deck mode (-bformat --deck X), `oscDeck` the truth's {deck} (A -> 1).
	// The desk's 3D for this deck, 0..1; none for another deck, without the
	// truth's word, or for NaN.
	std::optional<float> parseDeckThreeD (const Words& words, const std::string& address, int oscDeck, float value);
	// What to say when deck `oscDeck` lies outside the truth's deck range
	// (Core will not hear it); empty inside the range or without one. The
	// instance starts either way (Ruling A8).
	std::string deckOutsideTruth (const Words& words, int oscDeck);
	// The hello's address for the deck; empty when the truth has no word for it.
	std::string deckHelloAddress (const Words& words, int oscDeck);

	// What a deck tells Core about its clock: the BPM as it plays (0 with no
	// track), whether it plays, whether it shows MASTER.
	struct Tempo
	{
		double bpm = 0.0;
		bool playing = false, master = false;
	};
	struct TempoSend
	{
		std::string address;
		float bpm = 0.0f;
		int playing = 0, master = 0;
	};
	// The tempo report for deck `oscDeck` (any number the truth's range
	// allows); none without the truth's word.
	std::optional<TempoSend> tempoMessage (const Words& words, int oscDeck, const Tempo& tempo);
	// The BPM a deck shows: the grid's tempo times its rate; 0 with no track
	// or no grid.
	double tempoOf (bool loaded, double gridBpm, double rate);

	// When a deck's tempo report goes out (Ruling A5): the first state at
	// once, then on a change of playing/master or of the BPM by more than
	// 0.01 against what Core last heard, and again every 2 s; never twice
	// within 100 ms, so a pitch fader on the move does not flood Core.
	class TempoReport
	{
	public:
		std::optional<Tempo> next (const Tempo& now, double nowMs);

		static constexpr double repeatMs = 2000.0, minGapMs = 100.0, bpmDeadband = 0.01;

	private:
		std::optional<Tempo> sent;
		double sentAtMs = 0.0;
	};

	struct Outgoing
	{
		std::string address, text;
	};
	// Everything a one-deck instance sends every 2 s: the deck hello with the
	// host, nothing without the word. No bus reports, no stem meters (those
	// are the two-deck instance's).
	std::vector<Outgoing> oneDeckSends (const Words& words, int oscDeck, const std::string& host);

	// The port deck `oscDeck` (1-based) listens on: the truth's stemdeck.osc
	// port + (deck - 1), so two decks on one host do not fight for one port.
	// -1 for a deck below 1 or a port that is none.
	int deckListenPort (int stemdeckOscPort, int oscDeck);

	unsigned maskOf (const std::array<bool, buses::count>& on);

	// The rms of `samples` values whose squares sum to `sumOfSquares`; 0 for none.
	float rmsOf (double sumOfSquares, long long samples);

	// The number of the meter called `name` in the truth's vu_meters (its
	// place, from 1), none if the truth does not name it.
	std::optional<int> meterNumber (const std::vector<std::string>& meters, const std::string& name);
	// The /vu address of the meter called `name`, none if the truth lacks it.
	std::optional<std::string> vuAddressNamed (const Words& words, const std::string& name);

	struct Level
	{
		float peak = 0.0f, rms = 0.0f;
	};

	// One channel's block: its peak magnitude and the sum of its squares.
	struct Block
	{
		float peak = 0.0f;
		double squares = 0.0;
		int samples = 0;
	};

	Block measure (const float* samples, int count);

	// A stem's block, measured after knob and mute, as it reaches a bus: the
	// deck fader and the bus trim on it (Buses.h). The meters /vu 41-48.
	Block sentToBus (const Block& afterKnob, float fader);

	// The same block before the deck fader, with the bus trim: the screen's
	// stem meters behind the volume fader (a DJ mixer's channel meter reads
	// pre-fader); at the top of the fader it equals sentToBus.
	Block beforeFader (const Block& afterKnob);

	// The stem meters, deck by deck and stem by stem: /vu/41-48.
	constexpr int stemMeters = 8;

	struct Meter
	{
		std::string address;
		Level level;
	};

	// One tick's meters for the desk, in the order they go into its one OSC
	// bundle: the stems, then the AUX pair (stem_aux_L, stem_aux_R) where the
	// truth names it. One bundle, not one datagram per meter: the desk pays
	// per datagram (rafjagger/stemdeck#6).
	// Whether this tick's meters go to the desk. Two StemDecks write the same
	// meters; a silent one sends nothing, so it never overwrites the playing
	// one's levels with zeros -- except one zero bundle as it falls silent, so
	// the desk's bars drop once (rafjagger/stemdeck#6). Silent: every peak
	// below -90 dBFS.
	class MeterGate
	{
	public:
		enum Decision { send, sendZeros, skip };

		static constexpr float silentBelow = 3.1622776e-5f; // -90 dBFS, linear

		Decision next (const std::vector<Level>& levels);

	private:
		bool wasSounding = false;
	};

	// Which stems' bus masks Core is told, and when: the one place that
	// decides it, whatever moved a stem -- a click, the panel, Core, the
	// Auto-DJ, a session restore. Each call returns the stems (buses::stemIndex)
	// to send now, with the masks it was given.
	//   - While the link is down nothing is sent.
	//   - When it comes up, every stem, once: whatever changed before could not
	//     be sent (a session restored, the Auto-DJ taking over the playing
	//     deck at start), and Core asks for everything only when our hello is
	//     news to it -- a restart within its minute is not (2026-10-14).
	//   - After that, every stem whose mask differs from what Core last heard,
	//     once; and `answered` even when it did not change, so a refused
	//     switch is answered too.
	class BusReports
	{
	public:
		std::vector<int> start (const buses::Masks& now);
		std::vector<int> changed (const buses::Masks& now, int answered = -1);
		// Core's recall, and with every hello (a lost datagram mended within
		// half a minute): every stem, while the link runs.
		std::vector<int> everything (const buses::Masks& now);
		bool running() const { return up; }

	private:
		bool up = false;
		buses::Masks told {};
	};

	std::vector<Meter> levelBundle (const Words& words, const std::array<Level, stemMeters>& stems,
									const std::array<Level, 2>& aux);

	// A meter gathered block by block on the audio thread and emptied by the
	// sender. Lock-free and allocation-free; only one thread adds, so a pop in
	// between loses at most one block -- the same terms as the stem meters.
	class LevelTap
	{
	public:
		void add (const Block& block);
		Level pop();

	private:
		std::atomic<float> peak { 0.0f };
		std::atomic<double> squares { 0.0 };
		std::atomic<long long> samples { 0 };
	};
}
