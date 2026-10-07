#pragma once

#include <string>

// The output buses and who reaches them. Pure: no JUCE, testable.
//
// Five stereo buses, 1-4 and AUX, all feeding A³ Core. Every stem has one
// switch per bus and may be on several at once. Buses 1-4 are the desk's
// channels, switched by remote control (spec stemdeck-remote), and a stem
// there silences that channel's analog input -- so a fresh StemDeck starts
// with every stem on AUX and on no channel. Every bus is post fader; stem
// knob and mute act on every bus.
//
// No CUE bus and no PHONES outputs since 2026-10-07: the cue is the desk
// channel after its whole chain, in REAPER, whatever its input is.
namespace buses
{
	constexpr int count = 5;
	constexpr int aux = 4;

	constexpr unsigned allMask = (1u << count) - 1;

	// Which buses a stem starts on: the aux return only.
	constexpr unsigned defaultMask (int) { return 1u << aux; }

	// A mask as a session stored it: an older one may carry the CUE bus
	// (bit 5), which is dropped; every other switch is kept.
	constexpr unsigned fromStored (unsigned mask) { return mask & allMask; }

	// The gain from a stem into a bus: `onBus` is the stem's switch for it,
	// `fader` the channel fader.
	constexpr float gain (bool onBus, float fader) { return onBus ? fader : 0.0f; }

	// The fixed trim on every bus, linear: AUX is 6 dB down, so both decks on
	// the return (two full tracks, up to +5.9 dBFS) stay below full scale,
	// and the desk channels match it, so a stem is as loud on a channel as on
	// the return. Exactly one half, -6.02 dB, so two in-phase full-scale
	// peaks still land on 1.0 and not 0.02 dB over it.
	constexpr float trim = 0.5f;

	// "1" .. "4", "AUX": button labels and meter captions.
	inline std::string name (int bus)
	{
		return bus == aux ? "AUX" : std::to_string (bus + 1);
	}

	// JACK port base names: deck1 .. deck4 (kept from the four-bus days), aux.
	inline std::string portName (int bus)
	{
		return bus == aux ? "aux" : "deck" + std::to_string (bus + 1);
	}
}
