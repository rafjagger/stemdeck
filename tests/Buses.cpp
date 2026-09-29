#include <gtest/gtest.h>

#include "Buses.h"

TEST (Buses, SixBusesPhonesLast)
{
	EXPECT_EQ (buses::count, 6);
	EXPECT_EQ (buses::name (0), "1");
	EXPECT_EQ (buses::name (3), "4");
	EXPECT_EQ (buses::name (buses::aux), "AUX");
	EXPECT_EQ (buses::name (buses::phones), "PH");
	EXPECT_EQ (buses::portName (0), "deck1") << "the four-bus port names stay, so connections survive";
	EXPECT_EQ (buses::portName (buses::aux), "aux");
	EXPECT_EQ (buses::portName (buses::phones), "phones");
}

TEST (Buses, StemNStartsOnBusN)
{
	for (int stem = 0; stem < 4; ++stem)
		EXPECT_EQ (buses::defaultMask (stem), 1u << stem);
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
