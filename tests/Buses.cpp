#include <gtest/gtest.h>

#include "Buses.h"

#include <cmath>

// No CUE bus since 2026-10-07: the cue is the desk channel after its whole
// chain, in REAPER, whatever its input is.
TEST (Buses, FiveBusesAuxLast)
{
	EXPECT_EQ (buses::count, 5);
	EXPECT_EQ (buses::aux, buses::count - 1);
	EXPECT_EQ (buses::name (0), "1");
	EXPECT_EQ (buses::name (3), "4");
	EXPECT_EQ (buses::name (buses::aux), "AUX");
	EXPECT_EQ (buses::portName (0), "deck1") << "the four-bus port names stay, so connections survive";
	EXPECT_EQ (buses::portName (buses::aux), "aux");
}

// A session saved before 2026-10-07 may have a stem on the old CUE bus
// (bit 5); it loads with that bit dropped and every other switch kept.
TEST (Buses, AStoredMaskLosesTheOldCueBit)
{
	EXPECT_EQ (buses::fromStored (0b110001u), 0b010001u);
	EXPECT_EQ (buses::fromStored (0b100000u), 0u);
	EXPECT_EQ (buses::fromStored (0b011111u), 0b011111u);
	EXPECT_EQ (buses::fromStored (~0u), (1u << buses::count) - 1);
}

// Since the desk switches the channels by remote control (spec
// stemdeck-remote), a stem on bus N takes desk channel N and silences its
// analog input. A fresh StemDeck must not take all four: its stems start on
// the aux return and on no channel (final review 2026-10-02).
TEST (Buses, StemsStartOnTheReturnAndOnNoChannel)
{
	for (int stem = 0; stem < 4; ++stem)
		EXPECT_EQ (buses::defaultMask (stem), 1u << buses::aux);
}

TEST (Buses, EveryBusIsPostFader)
{
	EXPECT_FLOAT_EQ (buses::gain (true, 0.5f), 0.5f);
	EXPECT_FLOAT_EQ (buses::gain (true, 0.25f), 0.25f);
	EXPECT_FLOAT_EQ (buses::gain (false, 1.0f), 0.0f);
}

// The AUX bus can carry both decks at once (Core's STEM return), and the four
// stems of a set already sum to the track's own peak: two full tracks reach
// +5.9 dBFS. A fixed 6 dB trim on AUX keeps that below full scale (decided
// 2026-10-04). The desk channels get the same trim, so a stem plays as loud
// on a channel as on the return (decided the same evening).
TEST (Buses, ChannelsAndAuxAreTrimmedBySixDecibels)
{
	EXPECT_NEAR (20.0f * std::log10 (buses::trim), -6.0f, 0.05f);
}

TEST (Buses, TwoFullScaleDecksOnAuxStayBelowFullScale)
{
	const float twoSources[] = { 1.0f, 1.0f };
	float aux = 0.0f;
	for (const auto source : twoSources)
		aux += source;

	EXPECT_LE (aux * buses::trim, 1.0f) << "two in-phase full-scale peaks";

	const auto measuredTwoTracks = std::pow (10.0f, 5.9f / 20.0f);
	EXPECT_LT (measuredTwoTracks * buses::trim, 1.0f) << "the +5.9 dBFS measured 2026-10-04";
}

// One stem per bus, the rest on AUX (decided 2026-10-07): each of the buses
// 1-4 carries at most one of the eight stems -- both decks, since a desk
// channel can play any of them -- and a stem is on exactly one of bus 1, 2,
// 3, 4 or AUX.
namespace
{
	constexpr unsigned onAux = 1u << buses::aux;
	constexpr unsigned onBus (int bus) { return 1u << bus; }

	buses::Masks allOnAux()
	{
		buses::Masks masks;
		masks.fill (onAux);
		return masks;
	}

	int stemsOn (const buses::Masks& masks, int bus)
	{
		int count = 0;
		for (const auto mask : masks)
			count += (mask >> bus) & 1u;
		return count;
	}

	void expectTheRule (const buses::Masks& masks)
	{
		for (int bus = 0; bus < buses::aux; ++bus)
			EXPECT_LE (stemsOn (masks, bus), 1) << "bus " << bus + 1;
		for (size_t stem = 0; stem < masks.size(); ++stem)
		{
			const auto mask = masks[stem];
			EXPECT_EQ (mask & ~buses::allMask, 0u) << "stem " << stem;
			EXPECT_EQ (__builtin_popcount (mask), 1) << "stem " << stem << " is on exactly one bus";
		}
	}
}

TEST (Buses, EightStemsAcrossBothDecks)
{
	EXPECT_EQ (buses::stemCount, 8);
	EXPECT_EQ (buses::stemIndex (0, 0), 0);
	EXPECT_EQ (buses::stemIndex (0, 3), 3);
	EXPECT_EQ (buses::stemIndex (1, 0), 4);
	EXPECT_EQ (buses::stemIndex (1, 3), 7);
}

TEST (Buses, ASwitchOntoABusPutsTheStemThereAndOffAux)
{
	const auto after = buses::applySwitch (allOnAux(), 2, 0, true);
	EXPECT_EQ (after[2], onBus (0));
	expectTheRule (after);
}

TEST (Buses, TwoStemsOnOneBusIsImpossible)
{
	auto masks = allOnAux();
	for (int stem = 0; stem < buses::stemCount; ++stem)
	{
		masks = buses::applySwitch (masks, stem, 0, true);
		EXPECT_EQ (stemsOn (masks, 0), 1);
		expectTheRule (masks);
	}
}

TEST (Buses, StemBOntoBusOneMovesStemAToAux)
{
	auto masks = buses::applySwitch (allOnAux(), buses::stemIndex (0, 0), 0, true);
	masks = buses::applySwitch (masks, buses::stemIndex (1, 2), 0, true);

	EXPECT_EQ (masks[(size_t) buses::stemIndex (1, 2)], onBus (0));
	EXPECT_EQ (masks[(size_t) buses::stemIndex (0, 0)], onAux) << "the other deck's stem left bus 1 for AUX";
	expectTheRule (masks);
}

TEST (Buses, AStemMovesFromOneBusToAnother)
{
	auto masks = buses::applySwitch (allOnAux(), 1, 0, true);
	masks = buses::applySwitch (masks, 1, 3, true);
	EXPECT_EQ (masks[1], onBus (3)) << "bus 1 is free again, not shared";
	EXPECT_EQ (stemsOn (masks, 0), 0);
	expectTheRule (masks);
}

TEST (Buses, SwitchingOffSendsTheStemToAux)
{
	auto masks = buses::applySwitch (allOnAux(), 5, 2, true);
	masks = buses::applySwitch (masks, 5, 2, false);
	EXPECT_EQ (masks[5], onAux);
	expectTheRule (masks);
}

TEST (Buses, SwitchingOffABusTheStemIsNotOnChangesNothing)
{
	auto masks = buses::applySwitch (allOnAux(), 5, 2, true);
	EXPECT_EQ (buses::applySwitch (masks, 5, 1, false), masks);
}

// AUX is the place a stem is when it is on no channel: switching AUX on takes
// the stem off its channel; switching AUX off has nowhere to send it, so the
// stem stays on AUX.
TEST (Buses, AuxOnTakesTheStemOffItsChannel)
{
	auto masks = buses::applySwitch (allOnAux(), 6, 1, true);
	masks = buses::applySwitch (masks, 6, buses::aux, true);
	EXPECT_EQ (masks[6], onAux);
	EXPECT_EQ (stemsOn (masks, 1), 0);
	expectTheRule (masks);
}

TEST (Buses, AuxOffKeepsTheStemOnAux)
{
	EXPECT_EQ (buses::applySwitch (allOnAux(), 0, buses::aux, false), allOnAux());

	auto onChannel = buses::applySwitch (allOnAux(), 0, 3, true);
	EXPECT_EQ (buses::applySwitch (onChannel, 0, buses::aux, false), onChannel);
}

TEST (Buses, AnythingOutOfRangeChangesNothing)
{
	const auto masks = allOnAux();
	EXPECT_EQ (buses::applySwitch (masks, -1, 0, true), masks);
	EXPECT_EQ (buses::applySwitch (masks, buses::stemCount, 0, true), masks);
	EXPECT_EQ (buses::applySwitch (masks, 0, buses::count, true), masks);
	EXPECT_EQ (buses::applySwitch (masks, 0, -1, true), masks);
}

// A session saved before 2026-10-07 could have several stems on one bus, one
// stem on several buses, a stem on a channel and AUX, or a stem on nothing.
// It loads into the rule: in the order A1-A4, B1-B4, the first stem on a bus
// keeps it; a stem on several keeps the lowest one still free; everything
// else goes to AUX.
TEST (Buses, ALegacySessionLoadsIntoTheRule)
{
	const buses::Masks legacy {
		onBus (0) | onAux,           // A1: bus 1 and AUX -> bus 1
		onBus (0),                   // A2: bus 1, taken -> AUX
		onBus (1) | onBus (2),       // A3: buses 2 and 3 -> bus 2
		0u,                          // A4: nowhere -> AUX
		onBus (1) | onBus (2),       // B1: 2 taken, 3 free -> bus 3
		onBus (3) | (1u << 5),       // B2: bus 4 plus the old CUE bit -> bus 4
		onAux,                       // B3: AUX stays
		onBus (3),                   // B4: bus 4, taken -> AUX
	};
	const buses::Masks expected { onBus (0), onAux, onBus (1), onAux, onBus (2), onBus (3), onAux, onAux };

	const auto after = buses::normalise (legacy);
	EXPECT_EQ (after, expected);
	expectTheRule (after);
}

TEST (Buses, NormalisingAStateInTheRuleChangesNothing)
{
	auto masks = buses::applySwitch (allOnAux(), 3, 0, true);
	masks = buses::applySwitch (masks, 4, 1, true);
	EXPECT_EQ (buses::normalise (masks), masks);
	EXPECT_EQ (buses::normalise (allOnAux()), allOnAux());
}

// Core hears every stem the switch moved, and the switched stem even when
// nothing changed, so a refused switch (AUX off) is answered too.
TEST (Buses, EveryMovedStemIsReported)
{
	const auto before = buses::applySwitch (allOnAux(), 0, 0, true);
	const auto after = buses::applySwitch (before, 6, 0, true);
	EXPECT_EQ (buses::toReport (before, after, 6), (std::vector<int> { 0, 6 }));
	EXPECT_EQ (buses::toReport (before, before, 2), (std::vector<int> { 2 }));
}
