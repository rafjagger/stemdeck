#pragma once

#include <array>
#include <string>
#include <vector>

// The output buses and who reaches them. Pure: no JUCE, testable.
//
// Five stereo buses, 1-4 and AUX, all feeding A³ Core. Every stem has one
// switch per bus, and since 2026-10-07 is on exactly one bus: each of the
// buses 1-4 carries at most one of the eight stems, and every stem on none
// of them is on AUX (applySwitch, normalise). Buses 1-4 are the desk's
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

	// The eight stems the rule spans, deck by deck: a desk channel can play
	// any stem of either deck.
	constexpr int decks = 2;
	constexpr int stemsPerDeck = 4;
	constexpr int stemCount = decks * stemsPerDeck;

	constexpr int stemIndex (int deck, int stem) { return deck * stemsPerDeck + stem; }

	using Masks = std::array<unsigned, stemCount>;

	// Any state brought into the rule. In stem order (deck A stems 1-4, then
	// deck B stems 1-4) the first stem on a bus keeps it; a stem on several
	// keeps the lowest one still free; every other stem goes to AUX.
	inline Masks normalise (const Masks& masks)
	{
		Masks result {};
		unsigned taken = 0;
		for (size_t stem = 0; stem < masks.size(); ++stem)
		{
			result[stem] = 1u << aux;
			for (int bus = 0; bus < aux; ++bus)
			{
				const auto bit = 1u << bus;
				if ((masks[stem] & bit) == 0 || (taken & bit) != 0)
					continue;
				result[stem] = bit;
				taken |= bit;
				break;
			}
		}
		return result;
	}

	// The masks after one switch, under the rule. A stem put on bus N takes
	// N's previous stem to AUX; a stem taken off its bus goes to AUX; AUX on
	// takes the stem off its channel; AUX off is refused, since a stem on no
	// bus is on AUX. A switch out of range changes nothing.
	inline Masks applySwitch (const Masks& masks, int stem, int bus, bool on)
	{
		if (stem < 0 || stem >= stemCount || bus < 0 || bus >= count)
			return masks;

		auto result = normalise (masks);
		auto& mask = result[(size_t) stem];
		const auto bit = 1u << bus;

		if (bus == aux)
		{
			if (on)
				mask = bit;
			return result;
		}
		if (! on)
		{
			if (mask == bit)
				mask = 1u << aux;
			return result;
		}
		for (auto& other : result)
			if (other == bit)
				other = 1u << aux;
		mask = bit;
		return result;
	}

	// The stems whose masks Core must hear after a switch of `switched`: every
	// one that changed, and `switched` itself even when it did not, so a
	// refused switch is answered too. In stem order.
	inline std::vector<int> toReport (const Masks& before, const Masks& after, int switched)
	{
		std::vector<int> stems;
		for (int stem = 0; stem < stemCount; ++stem)
			if (stem == switched || before[(size_t) stem] != after[(size_t) stem])
				stems.push_back (stem);
		return stems;
	}
}
