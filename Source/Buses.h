#pragma once

#include <array>
#include <string>
#include <vector>

// The output buses and who reaches them. Pure: no JUCE, testable.
//
// Five stereo buses, 1-4 and AUX, all feeding A³ Core. Every stem has one
// switch per bus, and since 2026-10-07 is on one bus at most: each of the
// buses 1-4 carries at most one of the eight stems, and a stem a switch takes
// off its channel goes to AUX (applySwitch, normalise). A stem can also be
// off -- on no bus, silent: the Auto-DJ's place for a stem not yet, or no
// longer, playing (route). A manual switch never puts a stem off and leaves an
// off stem off; while the Auto-DJ plays, the place a switch sends a stem to
// is off instead of AUX, so nothing reaches AUX then. Buses 1-4 are the desk's
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
	constexpr unsigned offMask = 0;   // on no bus

	// Where a switch sends a stem it takes off its channel: AUX, or, while
	// the Auto-DJ plays, off.
	enum class Spare { aux, off };

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

	// What a stem's samples are multiplied by on their way out of a bus it is
	// on: the fader, then the trim. The same on every bus.
	constexpr float sendGain (float fader) { return gain (true, fader) * trim; }

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

	constexpr unsigned spareMask (Spare spare) { return spare == Spare::aux ? 1u << aux : offMask; }

	// Any state brought into the rule. In stem order (deck A stems 1-4, then
	// deck B stems 1-4) the first stem on a bus keeps it; a stem on several
	// keeps the lowest one still free; an off stem stays off; every other
	// stem goes to the spare place.
	inline Masks normalise (const Masks& masks, Spare spare = Spare::aux)
	{
		Masks result {};
		unsigned taken = 0;
		for (size_t stem = 0; stem < masks.size(); ++stem)
		{
			const auto onAux = (masks[stem] & (1u << aux)) != 0;
			result[stem] = masks[stem] == offMask ? offMask : onAux ? 1u << aux : spareMask (spare);
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
	// N's previous stem to the spare place; a stem taken off its bus goes
	// there too; AUX on takes the stem off its channel; AUX off is refused,
	// since a switch never puts a stem off. While the Auto-DJ plays (spare
	// off), AUX on is refused instead and AUX off puts the stem off. A switch
	// out of range changes nothing.
	inline Masks applySwitch (const Masks& masks, int stem, int bus, bool on, Spare spare = Spare::aux)
	{
		if (stem < 0 || stem >= stemCount || bus < 0 || bus >= count)
			return masks;

		auto result = normalise (masks, spare);
		auto& mask = result[(size_t) stem];
		const auto bit = 1u << bus;

		if (bus == aux)
		{
			if (spare == Spare::aux && on)
				mask = bit;
			if (spare == Spare::off && ! on && mask == bit)
				mask = offMask;
			return result;
		}
		if (! on)
		{
			if (mask == bit)
				mask = spareMask (spare);
			return result;
		}
		for (auto& other : result)
			if (other == bit)
				other = spareMask (spare);
		mask = bit;
		return result;
	}

	// The Auto-DJ's routing: a stem onto bus 1-4, or off. A stem it puts on a
	// bus takes that bus's previous stem off, never to AUX. In order; one out
	// of range is skipped.
	constexpr int off = -1;
	struct Route
	{
		int stem;   // stemIndex
		int bus;    // 0-3, or off
	};
	inline Masks route (const Masks& masks, const std::vector<Route>& routes)
	{
		auto result = masks;
		for (const auto& r : routes)
		{
			if (r.stem < 0 || r.stem >= stemCount || r.bus < off || r.bus >= aux)
				continue;
			if (r.bus == off)
			{
				result[(size_t) r.stem] = offMask;
				continue;
			}
			const auto bit = 1u << r.bus;
			for (auto& other : result)
				if (other == bit)
					other = offMask;
			result[(size_t) r.stem] = bit;
		}
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
