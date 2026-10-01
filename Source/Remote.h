#pragma once

#include <array>
#include <optional>
#include <string>

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
	};

	// One switch, 0-based inside StemDeck: deck 0-1, stem 0-3, bus 0-5.
	struct Switch
	{
		int deck = 0, stem = 0, bus = 0;
		bool on = false;
	};

	std::optional<Switch> parseSwitch (const Words& words, const std::string& address, int value);
	std::string reportAddress (const Words& words, int deck, int stem);
	bool isRecall (const Words& words, const std::string& address);
	std::string vuAddress (const Words& words, int deck, int stem);
	unsigned maskOf (const std::array<bool, 6>& on);

	// The rms of `samples` values whose squares sum to `sumOfSquares`; 0 for none.
	float rmsOf (double sumOfSquares, int samples);
}
