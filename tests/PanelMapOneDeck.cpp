// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "PanelMapOneDeck.h"

using panel1::Control;
using panel1::Event;
using panel1::InputDecoder;
using panel1::LedState;
using panel::Cell;
using panel::KeyState;
using panel::Rgb;
using Kind = Control::Kind;
using motionlite::PadState;
using motionlite::Shape;

namespace
{
	int buttonAt (Cell cell)
	{
		for (int i = 0; i < panel::buttonCount; ++i)
			if (panel::buttonCell (i) == cell)
				return i;
		return -1;
	}

	panel::Frame idleFrame()
	{
		panel::Frame frame;
		frame.buttons.fill (KeyState::idle);
		frame.encoders.fill ({ 0, KeyState::idle });
		return frame;
	}

	panel::Frame pressing (Cell cell, KeyState state = KeyState::held)
	{
		auto frame = idleFrame();
		frame.buttons[(size_t) buttonAt (cell)] = state;
		return frame;
	}

	LedState quietState()
	{
		LedState state;
		state.deckColour = { 31, 214, 205 };
		state.stemColours = { Rgb { 200, 80, 80 }, Rgb { 80, 200, 80 }, Rgb { 80, 80, 200 }, Rgb { 200, 200, 80 } };
		state.cueColour = { 255, 160, 0 };
		state.muteColour = { 220, 0, 0 };
		state.playColour = { 0, 220, 0 };
		return state;
	}

	Rgb ledAtCell (const panel::Leds& leds, int row, int col)
	{
		return leds[(size_t) panel::ledAt ({ row, col })];
	}
}

TEST (PanelMapOneDeck, TransportAndShiftKeysSitInTheEndColumns)
{
	EXPECT_EQ (panel1::controlAt ({ 0, 0 }).kind, Kind::cue);
	EXPECT_EQ (panel1::controlAt ({ 1, 0 }).kind, Kind::play);
	EXPECT_EQ (panel1::controlAt ({ 0, 9 }).kind, Kind::shift);
}

TEST (PanelMapOneDeck, UnassignedKeysAreNone)
{
	EXPECT_EQ (panel1::controlAt ({ 1, 9 }).kind, Kind::none);
	for (int row = 2; row < panel::rows; ++row)
		for (const int col : { 0, 9 })
			EXPECT_EQ (panel1::controlAt ({ row, col }).kind, Kind::none) << row << "," << col;
	// Not a key at all.
	EXPECT_EQ (panel1::controlAt ({ 0, 4 }).kind, Kind::none);
}

TEST (PanelMapOneDeck, EachStemHasEightPadsUnderItsEncoders)
{
	const std::array<std::array<Kind, 2>, 4> rows { { { Kind::orbit, Kind::ping }, { Kind::rise, Kind::scat },
													 { Kind::stemCue, Kind::stemMute }, { Kind::lengthDown, Kind::lengthUp } } };
	for (int stem = 0; stem < 4; ++stem)
		for (int r = 0; r < 4; ++r)
			for (int side = 0; side < 2; ++side)
			{
				const Cell cell { 2 + r, 2 * stem + 1 + side };
				const auto control = panel1::controlAt (cell);
				EXPECT_EQ (control.kind, rows[(size_t) r][(size_t) side]) << cell.row << "," << cell.col;
				EXPECT_EQ (control.stem, stem) << cell.row << "," << cell.col;
				EXPECT_EQ (panel1::cellOf (control), cell);
			}
}

TEST (PanelMapOneDeck, CellOfAnUnassignedControlIsNowhere)
{
	EXPECT_EQ (panel1::cellOf ({ Kind::none, -1 }), (Cell { -1, -1 }));
	EXPECT_EQ (panel1::cellOf ({ Kind::orbit, 4 }), (Cell { -1, -1 }));
	EXPECT_EQ (panel1::cellOf ({ Kind::cue, -1 }), (Cell { 0, 0 }));
	EXPECT_EQ (panel1::cellOf ({ Kind::shift, -1 }), (Cell { 0, 9 }));
}

TEST (PanelMapOneDeck, UpperEncoderIsGainLowerIs3D)
{
	for (int encoder = 0; encoder < panel::encoderCount; ++encoder)
	{
		const auto knob = panel1::encoderStem (encoder);
		EXPECT_EQ (knob.stem, encoder % 4);
		EXPECT_EQ (knob.threeD, encoder < 4) << encoder;
	}
	EXPECT_EQ (panel1::encoderStem (8).stem, -1);
	EXPECT_EQ (panel1::encoderStem (-1).stem, -1);
}

TEST (PanelMapOneDeck, KeysBecomeEventsOnPressOnly)
{
	InputDecoder decoder;
	auto events = decoder.feed (pressing ({ 2, 3 }));   // stem 1, ORBIT is col 3
	ASSERT_EQ (events.size(), 1u);
	EXPECT_EQ (events[0].type, Event::Type::shot);
	EXPECT_EQ (events[0].stem, 1);
	EXPECT_EQ (events[0].shape, Shape::orbit);
	// Still held: nothing more; let go: nothing for a shot.
	EXPECT_TRUE (decoder.feed (pressing ({ 2, 3 })).empty());
	EXPECT_TRUE (decoder.feed (idleFrame()).empty());

	events = decoder.feed (pressing ({ 3, 8 }));        // stem 3, SCAT
	ASSERT_EQ (events.size(), 1u);
	EXPECT_EQ (events[0].shape, Shape::scat);
	EXPECT_EQ (events[0].stem, 3);
}

TEST (PanelMapOneDeck, CueAndShiftReportReleaseToo)
{
	InputDecoder decoder;
	EXPECT_EQ (decoder.feed (pressing ({ 0, 0 })).at (0).type, Event::Type::cueDown);
	EXPECT_EQ (decoder.feed (idleFrame()).at (0).type, Event::Type::cueUp);
	EXPECT_EQ (decoder.feed (pressing ({ 0, 9 })).at (0).type, Event::Type::shiftDown);
	EXPECT_EQ (decoder.feed (idleFrame()).at (0).type, Event::Type::shiftUp);
	EXPECT_EQ (decoder.feed (pressing ({ 1, 0 })).at (0).type, Event::Type::play);
	EXPECT_TRUE (decoder.feed (idleFrame()).empty());
}

TEST (PanelMapOneDeck, LengthStemCueAndMuteKeys)
{
	InputDecoder decoder;
	EXPECT_EQ (decoder.feed (pressing ({ 5, 1 })).at (0).type, Event::Type::lengthDown);
	decoder.feed (idleFrame());
	EXPECT_EQ (decoder.feed (pressing ({ 5, 2 })).at (0).type, Event::Type::lengthUp);
	decoder.feed (idleFrame());
	EXPECT_EQ (decoder.feed (pressing ({ 4, 5 })).at (0).type, Event::Type::stemCue);
	decoder.feed (idleFrame());
	const auto mute = decoder.feed (pressing ({ 4, 6 }));
	EXPECT_EQ (mute.at (0).type, Event::Type::stemMute);
	EXPECT_EQ (mute.at (0).stem, 2);
}

TEST (PanelMapOneDeck, EncodersTurnGainAnd3DAndPushMutesOnlyFromTheUpperOne)
{
	InputDecoder decoder;
	auto frame = idleFrame();
	frame.encoders[5].delta = 3;    // upper, stem 1: gain
	frame.encoders[2].delta = -2;   // lower, stem 2: 3D
	auto events = decoder.feed (frame);
	ASSERT_EQ (events.size(), 2u);
	const auto find = [&] (Event::Type type) { for (const auto& e : events) if (e.type == type) return e; return Event {}; };
	EXPECT_EQ (find (Event::Type::gain).stem, 1);
	EXPECT_EQ (find (Event::Type::gain).value, 3.0);
	EXPECT_EQ (find (Event::Type::threeD).stem, 2);
	EXPECT_EQ (find (Event::Type::threeD).value, -2.0);

	frame = idleFrame();
	frame.encoders[6].key = KeyState::held;   // upper push, stem 2
	events = decoder.feed (frame);
	ASSERT_EQ (events.size(), 1u);
	EXPECT_EQ (events[0].type, Event::Type::stemMute);
	EXPECT_EQ (events[0].stem, 2);

	frame = idleFrame();
	frame.encoders[1].key = KeyState::click;   // lower push: nothing
	EXPECT_TRUE (decoder.feed (frame).empty());
}

TEST (PanelMapOneDeck, PotsAreUnused)
{
	InputDecoder decoder;
	auto frame = idleFrame();
	frame.pots = std::array<int, panel::potCount> { 0, 0, 0, 0 };
	decoder.feed (frame);
	frame.pots = std::array<int, panel::potCount> { 4000, 3000, 2000, 1000 };
	EXPECT_TRUE (decoder.feed (frame).empty());
}

//==============================================================================
TEST (PanelMapOneDeckLeds, EverythingAtRestIsTheDeckColourAndUnassignedIsDark)
{
	const auto state = quietState();
	const auto leds = panel1::leds (state, 0.0);
	const auto rest = panel::base (state.deckColour);
	EXPECT_EQ (ledAtCell (leds, 0, 0), rest);   // CUE
	EXPECT_EQ (ledAtCell (leds, 1, 0), rest);   // PLAY
	EXPECT_EQ (ledAtCell (leds, 0, 9), rest);   // SHIFT
	EXPECT_EQ (ledAtCell (leds, 2, 1), rest);
	EXPECT_EQ (ledAtCell (leds, 4, 8), rest);
	EXPECT_EQ (ledAtCell (leds, 1, 9), Rgb {});
	for (int row = 2; row < panel::rows; ++row)
		for (const int col : { 0, 9 })
			EXPECT_EQ (ledAtCell (leds, row, col), Rgb {}) << row << "," << col;
}

TEST (PanelMapOneDeckLeds, ARunningShotLightsInTheStemColour)
{
	auto state = quietState();
	state.stems[2].pads[(size_t) Shape::rise] = PadState::running;
	const auto leds = panel1::leds (state, 0.0);
	EXPECT_EQ (ledAtCell (leds, 3, 5), panel::vivid (state.stemColours[2]));   // stem 2 RISE: col 5
	EXPECT_EQ (ledAtCell (leds, 3, 6), panel::base (state.deckColour));        // its SCAT rests
}

TEST (PanelMapOneDeckLeds, AnArmedShotBlinksEvery500Ms)
{
	auto state = quietState();
	state.stems[0].pads[(size_t) Shape::orbit] = PadState::armed;
	const auto lit = panel::vivid (state.stemColours[0]);
	const auto rest = panel::base (state.deckColour);
	EXPECT_EQ (ledAtCell (panel1::leds (state, 10.0), 2, 1), lit);
	EXPECT_EQ (ledAtCell (panel1::leds (state, 10.30), 2, 1), rest);
	EXPECT_EQ (ledAtCell (panel1::leds (state, 10.50), 2, 1), lit);
	EXPECT_EQ (ledAtCell (panel1::leds (state, 10.80), 2, 1), rest);
}

TEST (PanelMapOneDeckLeds, CueAndMuteLightWhileOnAndTheEndsOfTheLengthRangeDim)
{
	auto state = quietState();
	state.stems[1].cued = true;
	state.stems[1].muted = true;
	state.stems[3].shortest = true;
	state.stems[3].longest = true;
	state.stems[0].longest = true;
	const auto leds = panel1::leds (state, 0.0);
	const auto rest = panel::base (state.deckColour);
	EXPECT_EQ (ledAtCell (leds, 4, 3), panel::vivid (state.cueColour));
	EXPECT_EQ (ledAtCell (leds, 4, 4), panel::vivid (state.muteColour));
	EXPECT_EQ (ledAtCell (leds, 4, 1), rest);

	const auto dim = panel::scaled (rest, 1, 4);
	EXPECT_EQ (ledAtCell (leds, 5, 7), dim);   // stem 3 shorter
	EXPECT_EQ (ledAtCell (leds, 5, 8), dim);   // stem 3 longer
	EXPECT_EQ (ledAtCell (leds, 5, 1), rest);  // stem 0 shorter still works
	EXPECT_EQ (ledAtCell (leds, 5, 2), dim);   // stem 0 longer is at the end
}

TEST (PanelMapOneDeckLeds, TransportAndShiftLightWhenOn)
{
	auto state = quietState();
	state.playing = true;
	state.atCue = true;
	state.shift = true;
	const auto leds = panel1::leds (state, 0.0);
	EXPECT_EQ (ledAtCell (leds, 0, 0), panel::vivid (state.cueColour));
	EXPECT_EQ (ledAtCell (leds, 1, 0), panel::vivid (state.playColour));
	EXPECT_EQ (ledAtCell (leds, 0, 9), panel::vivid (state.deckColour));
}
