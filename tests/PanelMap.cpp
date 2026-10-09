#include <gtest/gtest.h>

#include "PanelMap.h"

#include <algorithm>
#include <cmath>

using panel::Cell;
using panel::Control;
using panel::Event;
using panel::KeyState;

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

	panel::Frame withPots (std::array<int, panel::potCount> pots)
	{
		auto frame = idleFrame();
		frame.pots = pots;
		return frame;
	}

	void expectRoute (Cell cell, int deck, int stem, int bus)
	{
		const auto c = panel::controlAt (cell);
		EXPECT_EQ (c.kind, Control::Kind::route) << cell.row << "," << cell.col;
		EXPECT_EQ (c.deck, deck) << cell.row << "," << cell.col;
		EXPECT_EQ (c.stem, stem) << cell.row << "," << cell.col;
		EXPECT_EQ (c.bus, bus) << cell.row << "," << cell.col;
	}

	double saturation (panel::Rgb c)
	{
		const auto hi = std::max ({ c.r, c.g, c.b }), lo = std::min ({ c.r, c.g, c.b });
		return hi == 0 ? 0.0 : (hi - lo) / (double) hi;
	}

	double hue (panel::Rgb c)
	{
		const double r = c.r / 255.0, g = c.g / 255.0, b = c.b / 255.0;
		const auto hi = std::max ({ r, g, b }), lo = std::min ({ r, g, b }), d = hi - lo;
		double h = 0.0;
		if (hi == r)      h = std::fmod ((g - b) / d, 6.0);
		else if (hi == g) h = (b - r) / d + 2.0;
		else              h = (r - g) / d + 4.0;
		h *= 60.0;
		return h < 0.0 ? h + 360.0 : h;
	}

	int load (const panel::Leds& leds)
	{
		int total = 0;
		for (const auto& c : leds)
			total += c.r + c.g + c.b;
		return total;
	}

	const std::array<panel::Rgb, 4> stemColours { { { 0xe8, 0xa3, 0x3d }, { 0x4f, 0xb3, 0xbf },
													{ 0xd0, 0x6a, 0x86 }, { 0x8f, 0x9b, 0xd6 } } };

	panel::LedState allOnAux()
	{
		panel::LedState state;
		state.masks.fill (1u << buses::aux);
		state.stemColours = stemColours;
		state.playColour = { 0x3e, 0xc4, 0x6d };
		state.cueColour = { 0xe8, 0xa3, 0x3d };
		return state;
	}

	panel::Rgb at (const panel::Leds& leds, Cell cell) { return leds[(size_t) panel::ledAt (cell)]; }
	bool dark (panel::Rgb c) { return c.r == 0 && c.g == 0 && c.b == 0; }
}

//==============================================================================
// Each deck's half: the four stems as rows (the pads' rows 2-5), the buses
// as columns in the screen's order -- deck A AUX 1 2 3 4 from the left edge,
// deck B 1 2 3 4 AUX to the right edge; AUX is the end column.
TEST (PanelMap, PadsAreTheRoutingMatrix)
{
	expectRoute ({ 2, 0 }, 0, 0, buses::aux);
	expectRoute ({ 2, 1 }, 0, 0, 0);
	expectRoute ({ 3, 2 }, 0, 1, 1);
	expectRoute ({ 5, 4 }, 0, 3, 3);
	expectRoute ({ 2, 5 }, 1, 0, 0);
	expectRoute ({ 4, 8 }, 1, 2, 3);
	expectRoute ({ 5, 9 }, 1, 3, buses::aux);
}

TEST (PanelMap, EveryRouteHasExactlyOnePad)
{
	int routes = 0;
	for (int i = 0; i < panel::buttonCount; ++i)
	{
		const auto c = panel::controlAt (panel::buttonCell (i));
		if (c.kind != Control::Kind::route)
			continue;
		++routes;
		EXPECT_EQ (panel::cellOf (c), panel::buttonCell (i));
	}
	EXPECT_EQ (routes, buses::decks * buses::stemsPerDeck * buses::count);
}

// The end columns' top two keys: the deck's CUE over its PLAY, as on a CDJ.
TEST (PanelMap, EndColumnsTopKeysAreCueAndPlay)
{
	EXPECT_EQ (panel::controlAt ({ 0, 0 }).kind, Control::Kind::cue);
	EXPECT_EQ (panel::controlAt ({ 1, 0 }).kind, Control::Kind::play);
	EXPECT_EQ (panel::controlAt ({ 0, 0 }).deck, 0);
	EXPECT_EQ (panel::controlAt ({ 0, 9 }).kind, Control::Kind::cue);
	EXPECT_EQ (panel::controlAt ({ 1, 9 }).kind, Control::Kind::play);
	EXPECT_EQ (panel::controlAt ({ 1, 9 }).deck, 1);
	EXPECT_EQ (panel::controlAt ({ 0, 4 }).kind, Control::Kind::none) << "no key there";
}

//==============================================================================
TEST (PanelMap, APadSwitchesOnItsPressOnly)
{
	panel::InputDecoder decoder;
	const auto pressed = decoder.feed (pressing ({ 3, 2 }));
	ASSERT_EQ (pressed.size(), 1u);
	EXPECT_EQ (pressed[0].type, Event::Type::route);
	EXPECT_EQ (pressed[0].deck, 0);
	EXPECT_EQ (pressed[0].stem, 1);
	EXPECT_EQ (pressed[0].bus, 1);

	EXPECT_TRUE (decoder.feed (pressing ({ 3, 2 })).empty()) << "held is not pressed again";
	EXPECT_TRUE (decoder.feed (idleFrame()).empty()) << "the release does nothing";
	EXPECT_EQ (decoder.feed (pressing ({ 3, 2 })).size(), 1u);
}

// A press and release inside one poll arrives as a click: still one switch.
TEST (PanelMap, AClickIsOnePress)
{
	panel::InputDecoder decoder;
	EXPECT_EQ (decoder.feed (pressing ({ 2, 9 }, KeyState::click)).size(), 1u);
	EXPECT_TRUE (decoder.feed (pressing ({ 2, 9 }, KeyState::bounce)).empty());
}

TEST (PanelMap, CueIsHeldPlayToggles)
{
	panel::InputDecoder decoder;
	auto events = decoder.feed (pressing ({ 0, 9 }));
	ASSERT_EQ (events.size(), 1u);
	EXPECT_EQ (events[0].type, Event::Type::cueDown);
	EXPECT_EQ (events[0].deck, 1);
	events = decoder.feed (idleFrame());
	ASSERT_EQ (events.size(), 1u);
	EXPECT_EQ (events[0].type, Event::Type::cueUp);

	events = decoder.feed (pressing ({ 0, 0 }, KeyState::click));
	ASSERT_EQ (events.size(), 2u);
	EXPECT_EQ (events[0].type, Event::Type::cueDown);
	EXPECT_EQ (events[1].type, Event::Type::cueUp);

	events = decoder.feed (pressing ({ 1, 0 }));
	ASSERT_EQ (events.size(), 1u);
	EXPECT_EQ (events[0].type, Event::Type::play);
	EXPECT_EQ (events[0].deck, 0);
}

// Over each deck's half, two columns of two encoders: the four stems in
// reading order. The firmware counts the lower row first (0-3), then the
// upper (4-7), a channel -- a column pair of pads -- each.
TEST (PanelMap, EncodersTurnTheStemsGains)
{
	const auto turn = [] (int encoder, int delta)
	{
		panel::InputDecoder decoder;
		auto frame = idleFrame();
		frame.encoders[(size_t) encoder].delta = delta;
		return decoder.feed (frame);
	};

	const struct { int encoder, deck, stem; } expected[] = {
		{ 4, 0, 0 }, { 5, 0, 1 }, { 0, 0, 2 }, { 1, 0, 3 },
		{ 6, 1, 0 }, { 7, 1, 1 }, { 2, 1, 2 }, { 3, 1, 3 },
	};
	for (const auto& e : expected)
	{
		const auto events = turn (e.encoder, -2);
		ASSERT_EQ (events.size(), 1u) << e.encoder;
		EXPECT_EQ (events[0].type, Event::Type::gain);
		EXPECT_EQ (events[0].deck, e.deck) << e.encoder;
		EXPECT_EQ (events[0].stem, e.stem) << e.encoder;
		EXPECT_EQ (events[0].value, -2.0);
	}
}

TEST (PanelMap, AnEncoderPushMutesItsStem)
{
	panel::InputDecoder decoder;
	auto frame = idleFrame();
	frame.encoders[1].key = KeyState::held;
	auto events = decoder.feed (frame);
	ASSERT_EQ (events.size(), 1u);
	EXPECT_EQ (events[0].type, Event::Type::mute);
	EXPECT_EQ (events[0].deck, 0);
	EXPECT_EQ (events[0].stem, 3);
	EXPECT_TRUE (decoder.feed (frame).empty());
	EXPECT_TRUE (decoder.feed (idleFrame()).empty());
}

// The inner pot of each half is the deck's fader. Its first reading only
// says where it stands: a panel plugged in must not pull the fader there.
TEST (PanelMap, TheInnerPotIsTheFaderOnceMoved)
{
	panel::InputDecoder decoder;
	EXPECT_TRUE (decoder.feed (withPots ({ 100, 1000, 3000, 4000 })).empty());
	EXPECT_TRUE (decoder.feed (withPots ({ 100, 1004, 3000, 4000 })).empty()) << "noise";
	EXPECT_TRUE (decoder.feed (idleFrame()).empty()) << "a cycle without pots";

	auto events = decoder.feed (withPots ({ 900, 2047, 3000, 4000 }));
	ASSERT_EQ (events.size(), 1u) << "the outer pots do nothing";
	EXPECT_EQ (events[0].type, Event::Type::fader);
	EXPECT_EQ (events[0].deck, 0);
	EXPECT_NEAR (events[0].value, 0.5, 0.001);

	events = decoder.feed (withPots ({ 900, 2047, 4095, 4000 }));
	ASSERT_EQ (events.size(), 1u);
	EXPECT_EQ (events[0].deck, 1);
	EXPECT_DOUBLE_EQ (events[0].value, 1.0);

	decoder.reset();
	EXPECT_TRUE (decoder.feed (withPots ({ 0, 0, 0, 0 })).empty()) << "after a reconnect it stands somewhere new";
}

//==============================================================================
// Pastel UI colours wash out to near white on RGB LEDs: the hue is kept, the
// saturation pushed up, the brightness to full -- the budget scales after.
TEST (PanelMap, VividKeepsTheHueAndSaturates)
{
	for (const auto c : stemColours)
	{
		const auto v = panel::vivid (c);
		EXPECT_NEAR (hue (v), hue (c), 3.0);
		EXPECT_GE (saturation (v), 0.95);
		EXPECT_EQ (std::max ({ v.r, v.g, v.b }), 255);
	}
	EXPECT_TRUE (dark (panel::vivid ({ 0, 0, 0 })));
	const auto grey = panel::vivid ({ 100, 100, 100 });
	EXPECT_EQ (grey.r, grey.g);
	EXPECT_EQ (grey.g, grey.b);
	EXPECT_LT (saturation (panel::vivid ({ 200, 195, 190 })), 0.25) << "near grey stays near grey";
}

TEST (PanelMap, TheBudgetCapsTheTotalAndKeepsTheBalance)
{
	panel::Leds leds {};
	for (int i = 0; i < 8; ++i)
		leds[(size_t) i] = { 255, 255, 255 };
	EXPECT_EQ (panel::withinBudget (leds), leds) << "eight at full is the budget";

	for (auto& c : leds)
		c = { 255, 128, 0 };
	const auto scaled = panel::withinBudget (leds);
	EXPECT_LE (load (scaled), panel::budget);
	EXPECT_GT (load (scaled), panel::budget * 9 / 10);
	EXPECT_NEAR (scaled[0].g / (double) scaled[0].r, 128.0 / 255.0, 0.02);
}

TEST (PanelMap, EachStemLightsItsBusPad)
{
	auto state = allOnAux();
	state.masks[(size_t) buses::stemIndex (0, 2)] = 1u << 0;
	const auto leds = panel::render (state);

	EXPECT_EQ (panel::withinBudget (leds), leds) << "rendered within the budget";
	EXPECT_FALSE (dark (at (leds, { 4, 1 }))) << "deck A stem 3 on bus 1";
	EXPECT_TRUE (dark (at (leds, { 4, 0 }))) << "and no longer on AUX";
	EXPECT_NEAR (hue (at (leds, { 4, 1 })), hue (stemColours[2]), 3.0);
	EXPECT_FALSE (dark (at (leds, { 2, 9 }))) << "deck B stem 1 on AUX";
	EXPECT_NEAR (hue (at (leds, { 2, 9 })), hue (stemColours[0]), 3.0);

	int lit = 0;
	for (const auto& c : leds)
		lit += dark (c) ? 0 : 1;
	EXPECT_EQ (lit, 8) << "one pad per stem, transport dark";
}

TEST (PanelMap, AMutedStemIsDimmer)
{
	auto state = allOnAux();
	state.muted[(size_t) buses::stemIndex (1, 0)] = true;
	const auto leds = panel::render (state);
	const auto muted = at (leds, { 2, 9 }), open = at (leds, { 2, 0 });
	EXPECT_FALSE (dark (muted));
	EXPECT_LT (muted.r + muted.g + muted.b, (open.r + open.g + open.b) / 2);
}

TEST (PanelMap, TransportLightsAndEverythingStaysInBudget)
{
	auto state = allOnAux();
	state.playing = { true, false };
	state.atCue = { false, true };
	const auto leds = panel::render (state);
	EXPECT_FALSE (dark (at (leds, { 1, 0 })));
	EXPECT_TRUE (dark (at (leds, { 0, 0 })));
	EXPECT_TRUE (dark (at (leds, { 1, 9 })));
	EXPECT_FALSE (dark (at (leds, { 0, 9 })));

	state.playing = { true, true };
	state.atCue = { true, true };
	for (auto& c : state.stemColours)
		c = { 255, 255, 255 };
	EXPECT_LE (load (panel::render (state)), panel::budget);
}
