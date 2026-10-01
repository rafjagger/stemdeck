#pragma once

#include "Buses.h"

#include <string>

// What StemDeck puts on its outputs (spec stem-routing-on-the-desk, 2026-10-01).
// Buses: today's six stereo buses (Buses.h), mixed inside StemDeck.
// Stems: every stem of both decks on its own stereo pair at full level, for
// the A3 Mixer to mix -- deck A stems 1-4 on 1/2 ... 7/8, deck B on 9/10 ... 15/16.
// Pure: no JUCE, testable.
namespace outputs
{
	enum class Mode { Buses, Stems };

	inline int channelCount (Mode mode) { return mode == Mode::Stems ? 16 : buses::count * 2; }

	inline int stemChannel (int deck, int stem, int side) { return deck * 8 + stem * 2 + side; }

	inline std::string portName (Mode mode, int channel)
	{
		const auto side = channel % 2 == 0 ? "_L" : "_R";
		if (mode == Mode::Stems)
			return std::string (channel < 8 ? "a" : "b") + std::to_string ((channel % 8) / 2 + 1) + side;
		return buses::portName (channel / 2) + side;
	}

	inline Mode modeFromSetting (const std::string& value) { return value == "stems" ? Mode::Stems : Mode::Buses; }

	inline std::string settingValue (Mode mode) { return mode == Mode::Stems ? "stems" : "buses"; }

	// Status line: "16 OUT" or "6x ST" (with the multiplication sign, UTF-8).
	inline std::string statusLabel (Mode mode) { return mode == Mode::Stems ? "16 OUT" : "6\xc3\x97 ST"; }
}
