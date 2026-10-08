#pragma once

#include "Buses.h"

#include <array>
#include <atomic>
#include <optional>
#include <string>
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
