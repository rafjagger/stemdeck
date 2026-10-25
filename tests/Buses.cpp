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
			EXPECT_LE (__builtin_popcount (mask), 1) << "stem " << stem << " is on one bus at most";
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
	const auto after = buses::applySwitch (allOnAux(), 2, 2, true);
	EXPECT_EQ (after[2], onBus (2));
	expectTheRule (after);
}

TEST (Buses, TwoStemsOnOneBusIsImpossible)
{
	// Only stem 1 of either deck can reach bus 1; the other six are refused.
	auto masks = allOnAux();
	for (int stem = 0; stem < buses::stemCount; ++stem)
	{
		masks = buses::applySwitch (masks, stem, 0, true);
		EXPECT_EQ (stemsOn (masks, 0), 1);
		expectTheRule (masks);
	}
	EXPECT_EQ (masks[(size_t) buses::stemIndex (1, 0)], onBus (0));
	EXPECT_EQ (masks[(size_t) buses::stemIndex (0, 0)], onAux);
}

TEST (Buses, StemBOntoBusOneMovesStemAToAux)
{
	auto masks = buses::applySwitch (allOnAux(), buses::stemIndex (0, 0), 0, true);
	masks = buses::applySwitch (masks, buses::stemIndex (1, 0), 0, true);

	EXPECT_EQ (masks[(size_t) buses::stemIndex (1, 0)], onBus (0));
	EXPECT_EQ (masks[(size_t) buses::stemIndex (0, 0)], onAux) << "the other deck's stem left bus 1 for AUX";
	expectTheRule (masks);
}

TEST (Buses, AStemCannotMoveToAnotherChannel)
{
	auto masks = buses::applySwitch (allOnAux(), 1, 1, true);
	const auto before = masks;
	masks = buses::applySwitch (masks, 1, 3, true);
	EXPECT_EQ (masks, before) << "stem 2 reaches channel 2 and AUX only";
	expectTheRule (masks);
}

// Stem n of either deck reaches bus n and AUX, and nothing else (decided
// 2026-10-25).
TEST (Buses, StemNOnlyReachesChannelNAndAux)
{
	for (int stem = 0; stem < buses::stemCount; ++stem)
		for (int bus = 0; bus < buses::count; ++bus)
			EXPECT_EQ (buses::mayUse (stem, bus), bus == buses::aux || bus == stem % 4) << stem << "/" << bus;
	EXPECT_EQ (buses::channelOf (5), 1);
}

TEST (Buses, ASwitchOntoAnotherChannelIsRefused)
{
	EXPECT_EQ (buses::applySwitch (allOnAux(), buses::stemIndex (0, 3), 0, true), allOnAux());
	EXPECT_EQ (buses::applySwitch (allOnAux(), buses::stemIndex (0, 3), 0, true, buses::Spare::toOff), allOnAux());
}

// A session from before the rule may have a stem on a channel that is not
// its own: it loads on AUX, neither silent nor dropped.
TEST (Buses, AStemOnAnotherChannelLoadsOnAux)
{
	auto masks = allOnAux();
	masks[(size_t) buses::stemIndex (0, 0)] = 1u << 1;
	masks[(size_t) buses::stemIndex (1, 2)] = (1u << 2) | (1u << 0);
	const auto after = buses::normalise (masks);
	EXPECT_EQ (after[(size_t) buses::stemIndex (0, 0)], 1u << buses::aux);
	EXPECT_EQ (after[(size_t) buses::stemIndex (1, 2)], 1u << 2);
	expectTheRule (after);
}


TEST (Buses, SwitchingOffSendsTheStemToAux)
{
	auto masks = buses::applySwitch (allOnAux(), 5, 1, true);
	masks = buses::applySwitch (masks, 5, 1, false);
	EXPECT_EQ (masks[5], onAux);
	expectTheRule (masks);
}

TEST (Buses, SwitchingOffABusTheStemIsNotOnChangesNothing)
{
	auto masks = buses::applySwitch (allOnAux(), 5, 1, true);
	EXPECT_EQ (buses::applySwitch (masks, 5, 0, false), masks);
}

// AUX is the place a stem is when it is on no channel: switching AUX on takes
// the stem off its channel; switching AUX off has nowhere to send it, so the
// stem stays on AUX.
TEST (Buses, AuxOnTakesTheStemOffItsChannel)
{
	auto masks = buses::applySwitch (allOnAux(), 6, 2, true);
	masks = buses::applySwitch (masks, 6, buses::aux, true);
	EXPECT_EQ (masks[6], onAux);
	EXPECT_EQ (stemsOn (masks, 2), 0);
	expectTheRule (masks);
}

TEST (Buses, AuxOffKeepsTheStemOnAux)
{
	EXPECT_EQ (buses::applySwitch (allOnAux(), 0, buses::aux, false), allOnAux());

	auto onChannel = buses::applySwitch (allOnAux(), 0, 0, true);
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
// It loads into the rule: a stem keeps its own channel if it is on it and no
// earlier stem (A1-A4, B1-B4) holds it; a stem on nothing stays on nothing
// (the Auto-DJ's "off"); everything else, a foreign channel included, goes
// to AUX.
TEST (Buses, ALegacySessionLoadsIntoTheRule)
{
	const buses::Masks legacy {
		onBus (0) | onAux,           // A1: its own bus 1 and AUX -> bus 1
		onBus (0),                   // A2: bus 1 is not its own -> AUX
		onBus (1) | onBus (2),       // A3: buses 2 and 3, bus 3 is its own -> bus 3
		0u,                          // A4: nowhere -> stays nowhere
		onBus (0) | onBus (2),       // B1: bus 1 taken by A1, bus 3 foreign -> AUX
		onBus (1) | (1u << 5),       // B2: its own bus 2 plus the old CUE bit -> bus 2
		onAux,                       // B3: AUX stays
		onBus (3),                   // B4: its own bus 4 -> bus 4
	};
	const buses::Masks expected { onBus (0), onAux, onBus (2), buses::offMask, onAux, onBus (1), onAux, onBus (3) };

	const auto after = buses::normalise (legacy);
	EXPECT_EQ (after, expected);
	expectTheRule (after);
}

TEST (Buses, NormalisingAStateInTheRuleChangesNothing)
{
	auto masks = buses::applySwitch (allOnAux(), 3, 3, true);
	masks = buses::applySwitch (masks, 4, 0, true);
	EXPECT_EQ (buses::normalise (masks), masks);
	EXPECT_EQ (buses::normalise (allOnAux()), allOnAux());
}

// Core hears every stem the switch moved, and the switched stem even when
// nothing changed, so a refused switch (AUX off) is answered too.
TEST (Buses, EveryMovedStemIsReported)
{
	const auto before = buses::applySwitch (allOnAux(), 0, 0, true);
	const auto after = buses::applySwitch (before, 4, 0, true);
	EXPECT_EQ (buses::toReport (before, after, 4), (std::vector<int> { 0, 4 }));
	EXPECT_EQ (buses::toReport (before, before, 2), (std::vector<int> { 2 }));
}

// Core's own sequence when the desk hands channel 2 from stem S (A2) to stem
// X (B2), the only two that can use it: X bus 2 on, X AUX off, S bus 2 off.
// X takes the bus at the first switch and S goes to AUX; the late switches
// change nothing.
TEST (Buses, CoresHandoverOnChannelTwoNeverPassesAuxTwice)
{
	const int s = buses::stemIndex (0, 1), x = buses::stemIndex (1, 1);
	auto masks = buses::applySwitch (allOnAux(), s, 1, true);

	masks = buses::applySwitch (masks, x, 1, true);
	EXPECT_EQ (masks[(size_t) x], onBus (1));
	EXPECT_EQ (masks[(size_t) s], onAux);
	expectTheRule (masks);

	const auto settled = masks;
	masks = buses::applySwitch (masks, x, buses::aux, false);
	EXPECT_EQ (masks, settled);
	masks = buses::applySwitch (masks, s, 1, false);
	EXPECT_EQ (masks, settled) << "S already left bus 2";
}

// What a stem is multiplied by on its way out of any bus: fader, then trim.
// Both StemDeck's own strip meters and the desk's stem meters (/vu 41-48)
// show the stem through it, so the screen and the desk read alike
// (decided 2026-10-07).
TEST (Buses, AStemLeavesThroughFaderAndTrim)
{
	EXPECT_FLOAT_EQ (buses::sendGain (1.0f), 0.5f) << "top of the fader: the trim alone";
	EXPECT_NEAR (20.0f * std::log10 (buses::sendGain (std::pow (10.0f, -10.0f / 20.0f))), -16.02f, 0.01f);
	EXPECT_FLOAT_EQ (buses::sendGain (0.0f), 0.0f) << "fader closed";
}

// Off: on no bus at all, silent -- the Auto-DJ's place for a stem not yet (or
// no longer) playing. A manual switch never puts a stem there, but leaves one
// that is there alone.
namespace
{
	buses::Masks autoDjDeckAOn14()
	{
		auto masks = allOnAux();
		masks = buses::route (masks, { { 0, 0 }, { 1, 1 }, { 2, 2 }, { 3, 3 },
									   { 4, buses::off }, { 5, buses::off }, { 6, buses::off }, { 7, buses::off } });
		return masks;
	}
}

TEST (Buses, TheAutoDjRoutesADecksStemsOntoTheirBusesAndTheOtherOff)
{
	const auto masks = autoDjDeckAOn14();
	for (int s = 0; s < 4; ++s)
	{
		EXPECT_EQ (masks[(size_t) s], onBus (s));
		EXPECT_EQ (masks[(size_t) (4 + s)], buses::offMask);
	}
	EXPECT_EQ (stemsOn (masks, buses::aux), 0);
	expectTheRule (masks);
}

TEST (Buses, AHandoverTakesTheOldStemOffNotToAux)
{
	// B's bass onto bus 2 first: A's bass, displaced, goes off.
	const auto masks = buses::route (autoDjDeckAOn14(), { { 5, 1 }, { 1, buses::off } });
	EXPECT_EQ (masks[5], onBus (1));
	EXPECT_EQ (masks[1], buses::offMask);
	EXPECT_EQ (stemsOn (masks, buses::aux), 0);
	const auto otherOrder = buses::route (autoDjDeckAOn14(), { { 1, buses::off }, { 5, 1 } });
	EXPECT_EQ (otherOrder, masks) << "the order inside one handover does not matter";
}

TEST (Buses, AutoDjRouteOntoAnotherChannelIsSkipped)
{
	const auto m = autoDjDeckAOn14();
	EXPECT_EQ (buses::route (m, { { buses::stemIndex (1, 2), 0 } }), m);
}

TEST (Buses, ARouteOutOfRangeChangesNothing)
{
	const auto masks = autoDjDeckAOn14();
	EXPECT_EQ (buses::route (masks, { { -1, 0 }, { 8, 0 }, { 0, buses::aux }, { 0, 7 } }), masks);
}

TEST (Buses, AnOffStemSurvivesAManualSwitchElsewhere)
{
	auto masks = autoDjDeckAOn14();
	masks = buses::applySwitch (masks, 0, buses::aux, true);   // the DJ sends A's drums to AUX
	EXPECT_EQ (masks[0], onAux);
	EXPECT_EQ (masks[4], buses::offMask) << "B's drums stay off";
	EXPECT_EQ (buses::normalise (masks), masks);
}

TEST (Buses, AManualSwitchNeverPutsAStemOff)
{
	auto masks = buses::applySwitch (allOnAux(), 2, 2, true);
	masks = buses::applySwitch (masks, 2, 2, false);
	EXPECT_EQ (masks[2], onAux);
	EXPECT_EQ (buses::applySwitch (allOnAux(), 2, buses::aux, false), allOnAux()) << "AUX off is still refused";
	// An off stem switched onto AUX by hand plays there, as before.
	EXPECT_EQ (buses::applySwitch (autoDjDeckAOn14(), 6, buses::aux, true)[6], onAux);
}

// While the Auto-DJ plays, a displaced stem goes off, not to AUX.
TEST (Buses, WhileTheAutoDjPlaysTheSparePlaceIsOff)
{
	const auto masks = autoDjDeckAOn14();
	const auto displaced = buses::applySwitch (masks, 4, 0, true, buses::Spare::toOff);
	EXPECT_EQ (displaced[4], onBus (0));
	EXPECT_EQ (displaced[0], buses::offMask) << "A's drums, displaced, go off";

	const auto takenOff = buses::applySwitch (masks, 1, 1, false, buses::Spare::toOff);
	EXPECT_EQ (takenOff[1], buses::offMask);

}

// A DJ's AUX press while the Auto-DJ plays is his: the stem leaves its desk
// bus for AUX, and AUX off puts it on no bus, for the Auto-DJ to place.
TEST (Buses, WhileTheAutoDjPlaysTheDjCanStillUseAux)
{
	const auto masks = autoDjDeckAOn14();
	const auto onReturn = buses::applySwitch (masks, 2, buses::aux, true, buses::Spare::toOff);
	EXPECT_EQ (onReturn[2], onAux) << "A's other on the return";
	EXPECT_EQ (stemsOn (onReturn, 2), 0) << "bus 3 carries nothing now";
	expectTheRule (onReturn);

	const auto back = buses::applySwitch (onReturn, 2, buses::aux, false, buses::Spare::toOff);
	EXPECT_EQ (back[2], buses::offMask);
}

// CUE is a send beside the buses, not a sixth bus: it has no bit in a stem's
// mask, so the one-stem-per-bus rule, the Auto-DJ's routing and what Core is
// told never see it.
TEST (Buses, CueIsNotABus)
{
	EXPECT_EQ (buses::count, 5);
	EXPECT_EQ (buses::allMask, 0b11111u);
}

TEST (Buses, NoSwitchOrRouteEverSetsABitBeyondTheBuses)
{
	buses::Masks masks;
	masks.fill (buses::allMask | 0b100000u);
	for (const auto mask : buses::normalise (masks))
		EXPECT_EQ (mask & ~buses::allMask, 0u);
	for (const auto mask : buses::applySwitch (masks, 3, 3, true))
		EXPECT_EQ (mask & ~buses::allMask, 0u);
	for (const auto mask : buses::route (buses::normalise (masks), { { 0, 0 }, { 1, buses::off } }))
		EXPECT_EQ (mask & ~buses::allMask, 0u);
}

// The cue takes the stem straight after its knob: unity knob gives what a
// stem has on a bus at fader 0 dB (the same trim), whatever the fader and
// the mute do, and a stem that is not on cue sends nothing.
TEST (Buses, CueSendsTheStemAfterItsKnobAndTrim)
{
	EXPECT_FLOAT_EQ (buses::cueSendGain (true, 1.0f), buses::sendGain (1.0f));
	EXPECT_FLOAT_EQ (buses::cueSendGain (true, 0.5f), 0.25f);
	EXPECT_FLOAT_EQ (buses::cueSendGain (false, 1.0f), 0.0f);
}

TEST (Buses, ACuedStemIsHeardWhileMutedAndWhateverTheFader)
{
	EXPECT_FLOAT_EQ (buses::busSendGain (true, 1.0f, true, 1.0f), 0.0f) << "muted: silent on a bus";
	EXPECT_GT (buses::cueSendGain (true, 1.0f), 0.0f) << "but on cue";
	EXPECT_FLOAT_EQ (buses::busSendGain (true, 1.0f, false, 0.0f), 0.0f) << "fader closed: silent on a bus";
	EXPECT_FLOAT_EQ (buses::busSendGain (true, 1.0f, false, 1.0f), buses::cueSendGain (true, 1.0f));
}
