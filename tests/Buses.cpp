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
