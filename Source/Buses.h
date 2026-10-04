#pragma once

#include <string>

// The output buses and who reaches them. Pure: no JUCE, testable.
//
// Six stereo buses: 1-4 and AUX feed A³ Core, PHONES is for the headphones.
// Every stem has one switch per bus and may be on several at once. Buses 1-4
// are the desk's channels, switched by remote control (spec stemdeck-remote),
// and a stem there silences that channel's analog input -- so a fresh
// StemDeck starts with every stem on AUX and on no channel. The program buses (1-4, AUX) are post fader;
// PHONES is pre fader and also takes the whole deck when its PHONES button
// is on. Stem knob and mute act on every bus.
namespace buses
{
	constexpr int count = 6;
	constexpr int aux = 4;
	constexpr int phones = 5;

	// Which buses a stem starts on: the aux return only.
	constexpr unsigned defaultMask (int) { return 1u << aux; }

	// The gain from a stem into `bus`: `onBus` is the stem's switch for it,
	// `deckPhones` the deck's PHONES button, `fader` the channel fader.
	constexpr float gain (int bus, bool onBus, bool deckPhones, float fader)
	{
		if (bus == phones)
			return onBus || deckPhones ? 1.0f : 0.0f;
		return onBus ? fader : 0.0f;
	}

	// The fixed trim on a summed bus, linear: AUX is 6 dB down, so both decks
	// on the return (two full tracks, up to +5.9 dBFS) stay below full scale;
	// the channels and CUE are untouched. Exactly one half, -6.02 dB, so two
	// in-phase full-scale peaks still land on 1.0 and not 0.02 dB over it.
	constexpr float trimFor (int bus) { return bus == aux ? 0.5f : 1.0f; }

	// "1" .. "4", "AUX", "CUE": button labels and meter captions (PH until
	// 2026-10-01, when PFL became cue across the A3 system).
	inline std::string name (int bus)
	{
		return bus == aux ? "AUX" : bus == phones ? "CUE" : std::to_string (bus + 1);
	}

	// JACK port base names: deck1 .. deck4 (kept from the four-bus days), aux, phones.
	inline std::string portName (int bus)
	{
		return bus == aux ? "aux" : bus == phones ? "phones" : "deck" + std::to_string (bus + 1);
	}
}
