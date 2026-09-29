#pragma once

#include <string>

// The output buses and who reaches them. Pure: no JUCE, testable.
//
// Six stereo buses: 1-4 and AUX feed A³ Core, PHONES is for the headphones.
// Every stem has one switch per bus and may be on several at once; a new set
// starts with stem N on bus N. The program buses (1-4, AUX) are post fader;
// PHONES is pre fader and also takes the whole deck when its PHONES button
// is on. Stem knob and mute act on every bus.
namespace buses
{
	constexpr int count = 6;
	constexpr int aux = 4;
	constexpr int phones = 5;

	// Which buses a stem starts on: bus N for stem N.
	constexpr unsigned defaultMask (int stem) { return 1u << stem; }

	// The gain from a stem into `bus`: `onBus` is the stem's switch for it,
	// `deckPhones` the deck's PHONES button, `fader` the channel fader.
	constexpr float gain (int bus, bool onBus, bool deckPhones, float fader)
	{
		if (bus == phones)
			return onBus || deckPhones ? 1.0f : 0.0f;
		return onBus ? fader : 0.0f;
	}

	// "1" .. "4", "AUX", "PH": button labels and meter captions.
	inline std::string name (int bus)
	{
		return bus == aux ? "AUX" : bus == phones ? "PH" : std::to_string (bus + 1);
	}

	// JACK port base names: deck1 .. deck4 (kept from the four-bus days), aux, phones.
	inline std::string portName (int bus)
	{
		return bus == aux ? "aux" : bus == phones ? "phones" : "deck" + std::to_string (bus + 1);
	}
}
