#include <gtest/gtest.h>

#include "Scs3d.h"

#include <algorithm>

using scs3d::Event;

TEST (Scs3d, TopButtons1256MuteTheStems)
{
	const std::uint8_t buttons[] = { 0x20, 0x22, 0x28, 0x2A };   // 1 2 5 6
	for (int s = 0; s < 4; ++s)
	{
		const auto e = scs3d::decode (0x90, buttons[s], 1);
		EXPECT_EQ (e.type, Event::Type::mute);
		EXPECT_EQ (e.stem, s);
	}
	EXPECT_EQ (scs3d::decode (0x80, 0x20, 0).type, Event::Type::none) << "only the press toggles";
}

TEST (Scs3d, Buttons3And4Loop)
{
	EXPECT_EQ (scs3d::decode (0x90, 0x24, 1).type, Event::Type::loopInOut);
	EXPECT_EQ (scs3d::decode (0x90, 0x26, 1).type, Event::Type::loopToggle);
}

TEST (Scs3d, AroundTheCircleTheLibrary)
{
	EXPECT_EQ (scs3d::decode (0x90, 0x2C, 1).type, Event::Type::previous);
	EXPECT_EQ (scs3d::decode (0x90, 0x2E, 1).type, Event::Type::next);
	EXPECT_EQ (scs3d::decode (0x90, 0x01, 1).type, Event::Type::load) << "a tap in the centre";
	EXPECT_EQ (scs3d::decode (0x90, 0x62, 1).type, Event::Type::scratchTouch) << "the ring still scratches";
}

TEST (Scs3d, TransportAndCueRelease)
{
	EXPECT_EQ (scs3d::decode (0x90, 0x6D, 1).type, Event::Type::play);
	EXPECT_EQ (scs3d::decode (0x90, 0x6E, 1).type, Event::Type::cueDown);
	EXPECT_EQ (scs3d::decode (0x80, 0x6E, 0).type, Event::Type::cueUp);
	EXPECT_EQ (scs3d::decode (0x90, 0x6E, 0).type, Event::Type::cueUp) << "note on at 0 is a release too";
	EXPECT_EQ (scs3d::decode (0x90, 0x6F, 1).type, Event::Type::sync);
	EXPECT_EQ (scs3d::decode (0x90, 0x70, 1).type, Event::Type::master);
}

TEST (Scs3d, SlidersAndTheCircle)
{
	EXPECT_DOUBLE_EQ (scs3d::decode (0xB0, 0x07, 127).value, 1.0);
	EXPECT_EQ (scs3d::decode (0xB0, 0x07, 0).type, Event::Type::gain);
	const auto pitch = scs3d::decode (0xB0, 0x04, 61);
	EXPECT_EQ (pitch.type, Event::Type::pitch);
	EXPECT_DOUBLE_EQ (pitch.value, -3.0);
	EXPECT_EQ (scs3d::decode (0x90, 0x62, 1).type, Event::Type::scratchTouch);
	const auto move = scs3d::decode (0xB0, 0x63, 66);
	EXPECT_EQ (move.type, Event::Type::scratchMove);
	EXPECT_DOUBLE_EQ (move.value, 2.0);
	EXPECT_EQ (scs3d::decode (0x80, 0x62, 0).type, Event::Type::scratchRelease);
	EXPECT_EQ (scs3d::decode (0xB0, 0x03, 50).type, Event::Type::none) << "absolute pitch is ignored: relative only";
}

namespace
{
	int valueOf (const std::vector<scs3d::Message>& messages, std::uint8_t id)
	{
		int value = -1;
		for (const auto& m : messages)
			if (m[1] == id)
				value = m[2];
		return value;
	}
}

TEST (Scs3d, MutedIsRedPlayingIsBlue)
{
	scs3d::Leds leds;
	leds.muted = { true, false, false, true };
	const auto m = scs3d::render (leds);
	EXPECT_EQ (valueOf (m, 0x20), 1);
	EXPECT_EQ (valueOf (m, 0x22), 2);
	EXPECT_EQ (valueOf (m, 0x2A), 1) << "stem 4 on button 6";
}

TEST (Scs3d, TheDeckLightSaysWhichDeck)
{
	scs3d::Leds leds;
	leds.deck = 1;
	const auto m = scs3d::render (leds);
	EXPECT_EQ (valueOf (m, 0x71), 0);
	EXPECT_EQ (valueOf (m, 0x72), 1);
}

TEST (Scs3d, TheMetersShowFaderPitchAndPlatter)
{
	scs3d::Leds leds;
	leds.gain = 1.0;
	leds.pitch = 0.0;
	leds.platter = 0.26;
	const auto m = scs3d::render (leds);

	int gainLit = 0, pitchLit = 0, circleLit = 0;
	for (std::uint8_t id = 0x34; id < 0x34 + 9; ++id) gainLit += valueOf (m, id);
	for (std::uint8_t id = 0x3F; id < 0x3F + 9; ++id) pitchLit += valueOf (m, id);
	for (std::uint8_t id = 0x5D; id < 0x5D + 16; ++id) circleLit += valueOf (m, id);
	EXPECT_EQ (gainLit, 9) << "fader up: the whole bar";
	EXPECT_EQ (pitchLit, 1) << "no pitch: only the middle";
	EXPECT_EQ (circleLit, 1) << "one needle";

	leds.gain = 0.0;
	leds.pitch = -1.0;
	const auto low = scs3d::render (leds);
	gainLit = 0; pitchLit = 0;
	for (std::uint8_t id = 0x34; id < 0x34 + 9; ++id) gainLit += valueOf (low, id);
	for (std::uint8_t id = 0x3F; id < 0x3F + 9; ++id) pitchLit += valueOf (low, id);
	EXPECT_EQ (gainLit, 0);
	EXPECT_EQ (pitchLit, 5) << "middle to the end";
}
