#include <gtest/gtest.h>

#include "Buses.h"

#include <cmath>

TEST (Buses, SixBusesPhonesLast)
{
	EXPECT_EQ (buses::count, 6);
	EXPECT_EQ (buses::name (0), "1");
	EXPECT_EQ (buses::name (3), "4");
	EXPECT_EQ (buses::name (buses::aux), "AUX");
	EXPECT_EQ (buses::name (buses::phones), "CUE");   // PH until 2026-10-01: PFL became cue everywhere
	EXPECT_EQ (buses::portName (0), "deck1") << "the four-bus port names stay, so connections survive";
	EXPECT_EQ (buses::portName (buses::aux), "aux");
	EXPECT_EQ (buses::portName (buses::phones), "phones");
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

TEST (Buses, ProgramBusesArePostFader)
{
	EXPECT_FLOAT_EQ (buses::gain (0, true, false, 0.5f), 0.5f);
	EXPECT_FLOAT_EQ (buses::gain (buses::aux, true, false, 0.25f), 0.25f);
	EXPECT_FLOAT_EQ (buses::gain (2, false, true, 1.0f), 0.0f) << "deck PHONES reaches only PHONES";
}

TEST (Buses, PhonesIsPreFaderFromTheStemOrTheDeck)
{
	EXPECT_FLOAT_EQ (buses::gain (buses::phones, true, false, 0.0f), 1.0f) << "fader down, still in the phones";
	EXPECT_FLOAT_EQ (buses::gain (buses::phones, false, true, 0.0f), 1.0f);
	EXPECT_FLOAT_EQ (buses::gain (buses::phones, true, true, 0.3f), 1.0f) << "both: once, not twice";
	EXPECT_FLOAT_EQ (buses::gain (buses::phones, false, false, 1.0f), 0.0f);
}

// The AUX bus can carry both decks at once (Core's STEM return), and the four
// stems of a set already sum to the track's own peak: two full tracks reach
// +5.9 dBFS. A fixed 6 dB trim on AUX keeps that below full scale (decided
// 2026-10-04). The desk channels get the same trim, so a stem plays as loud
// on a channel as on the return (decided the same evening); CUE stays as it is.
TEST (Buses, ChannelsAndAuxAreTrimmedBySixDecibels)
{
	EXPECT_NEAR (20.0f * std::log10 (buses::trimFor (buses::aux)), -6.0f, 0.05f);
	for (int bus = 0; bus < buses::aux; ++bus)
		EXPECT_FLOAT_EQ (buses::trimFor (bus), buses::trimFor (buses::aux)) << "desk channel " << bus + 1;
	EXPECT_FLOAT_EQ (buses::trimFor (buses::phones), 1.0f) << "CUE";
}

TEST (Buses, TwoFullScaleDecksOnAuxStayBelowFullScale)
{
	const float twoSources[] = { 1.0f, 1.0f };
	float aux = 0.0f;
	for (const auto source : twoSources)
		aux += source;

	EXPECT_LE (aux * buses::trimFor (buses::aux), 1.0f) << "two in-phase full-scale peaks";

	const auto measuredTwoTracks = std::pow (10.0f, 5.9f / 20.0f);
	EXPECT_LT (measuredTwoTracks * buses::trimFor (buses::aux), 1.0f) << "the +5.9 dBFS measured 2026-10-04";
}
