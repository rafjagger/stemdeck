#include <gtest/gtest.h>

#include "PanelProtocol.h"

#include <algorithm>
#include <set>

using panel::Cell;
using panel::KeyState;

namespace
{
	// A poll reply as the firmware sends it: buttons, encoders, pots if asked.
	std::vector<std::uint8_t> reply (const std::array<KeyState, panel::buttonCount>& buttons,
									 const std::array<std::pair<int, KeyState>, panel::encoderCount>& encoders,
									 const std::array<int, panel::potCount>* pots, bool encodersFirst = false)
	{
		std::vector<std::uint8_t> b { panel::getButtons };
		b.resize (12, 0);
		for (int i = 0; i < panel::buttonCount; ++i)
			b[(size_t) (1 + i / 4)] |= (std::uint8_t) ((int) buttons[(size_t) i] << ((i % 4) * 2));

		std::vector<std::uint8_t> e { panel::getEncoders };
		for (const auto& [delta, key] : encoders)
		{
			const auto d = (std::uint16_t) (std::int16_t) delta;
			e.push_back ((std::uint8_t) (d & 0xff));
			e.push_back ((std::uint8_t) (d >> 8));
			e.push_back ((std::uint8_t) key);
		}

		std::vector<std::uint8_t> all = encodersFirst ? e : b;
		const auto& second = encodersFirst ? b : e;
		all.insert (all.end(), second.begin(), second.end());
		if (pots != nullptr)
		{
			all.push_back (panel::getPots);
			for (const auto p : *pots)
			{
				all.push_back ((std::uint8_t) (p & 0xff));
				all.push_back ((std::uint8_t) (p >> 8));
			}
		}
		return all;
	}

	std::array<KeyState, panel::buttonCount> allIdle()
	{
		std::array<KeyState, panel::buttonCount> keys;
		keys.fill (KeyState::idle);
		return keys;
	}

	std::array<std::pair<int, KeyState>, panel::encoderCount> stillEncoders()
	{
		std::array<std::pair<int, KeyState>, panel::encoderCount> encoders;
		encoders.fill ({ 0, KeyState::idle });
		return encoders;
	}
}

// One write asks for a whole cycle and one read takes the answer: buttons and
// encoders always, the pots on some cycles.
TEST (PanelProtocol, APollAsksForButtonsEncodersAndSometimesPots)
{
	EXPECT_EQ (panel::pollRequest (false), (std::vector<std::uint8_t> { 0x04, 0x03 }));
	EXPECT_EQ (panel::pollRequest (true), (std::vector<std::uint8_t> { 0x04, 0x03, 0x02 }));
	EXPECT_EQ (panel::pollReplySize (false), 37u);
	EXPECT_EQ (panel::pollReplySize (true), 46u);
}

TEST (PanelProtocol, AReplyIsUnpacked)
{
	auto buttons = allIdle();
	buttons[0] = KeyState::held;
	buttons[5] = KeyState::click;
	buttons[43] = KeyState::bounce;
	auto encoders = stillEncoders();
	encoders[2] = { -3, KeyState::idle };
	encoders[7] = { 300, KeyState::held };
	const std::array<int, panel::potCount> pots { 0, 4095, 2048, 17 };

	const auto bytes = reply (buttons, encoders, &pots);
	const auto frame = panel::parsePollReply (bytes.data(), bytes.size(), true);
	ASSERT_TRUE (frame.has_value());
	EXPECT_EQ (frame->buttons, buttons);
	EXPECT_EQ (frame->encoders[2].delta, -3);
	EXPECT_EQ (frame->encoders[7].delta, 300);
	EXPECT_EQ (frame->encoders[7].key, KeyState::held);
	ASSERT_TRUE (frame->pots.has_value());
	EXPECT_EQ (*frame->pots, pots);
}

// Some USB-CDC stacks hand the encoders' answer over first.
TEST (PanelProtocol, EncodersFirstIsReadAsWell)
{
	auto buttons = allIdle();
	buttons[10] = KeyState::held;
	buttons[33] = KeyState::click;
	auto encoders = stillEncoders();
	encoders[1] = { 5, KeyState::held };
	const auto bytes = reply (buttons, encoders, nullptr, true);
	const auto frame = panel::parsePollReply (bytes.data(), bytes.size(), false);
	ASSERT_TRUE (frame.has_value());
	EXPECT_EQ (frame->buttons, buttons);
	EXPECT_EQ (frame->encoders[1].delta, 5);
	EXPECT_EQ (frame->encoders[1].key, KeyState::held);
	EXPECT_FALSE (frame->pots.has_value());
}

// A slipped stream has its markers in the wrong place: refused, not misread.
TEST (PanelProtocol, ASlippedReplyIsRefused)
{
	auto bytes = reply (allIdle(), stillEncoders(), nullptr);
	bytes.insert (bytes.begin(), 0x01);
	bytes.pop_back();
	EXPECT_FALSE (panel::parsePollReply (bytes.data(), bytes.size(), false).has_value());

	const std::array<int, panel::potCount> pots {};
	auto withPots = reply (allIdle(), stillEncoders(), &pots);
	withPots[37] = 0x07;
	EXPECT_FALSE (panel::parsePollReply (withPots.data(), withPots.size(), true).has_value());
	EXPECT_FALSE (panel::parsePollReply (withPots.data(), 20, true).has_value()) << "short";
}

TEST (PanelProtocol, SetLedIsFiveBytesAndNoAnswer)
{
	EXPECT_EQ (panel::setLed (12, { 1, 2, 3 }), (std::array<std::uint8_t, 5> { 0x05, 12, 1, 2, 3 }));
}

// The keys by their place, from the firmware's two orders (host.py's
// BUTTON_LABELS and LED_BUTTON_LABELS): 44 keys, every place once, and the
// LED of a key is the one at its place.
TEST (PanelProtocol, EveryKeyHasOnePlaceAndOneLed)
{
	std::set<std::pair<int, int>> places;
	std::set<int> leds;
	for (int i = 0; i < panel::buttonCount; ++i)
	{
		const auto cell = panel::buttonCell (i);
		EXPECT_TRUE (panel::isKey (cell)) << i;
		places.insert ({ cell.row, cell.col });
		leds.insert (panel::ledAt (cell));
	}
	EXPECT_EQ (places.size(), 44u);
	EXPECT_EQ (leds.size(), 44u);
	EXPECT_EQ (*leds.begin(), 0);
	EXPECT_EQ (*leds.rbegin(), 43);

	EXPECT_FALSE (panel::isKey ({ 0, 3 })) << "the pots stand over the pads";
	EXPECT_TRUE (panel::isKey ({ 0, 0 }));
	EXPECT_TRUE (panel::isKey ({ 5, 9 }));
	EXPECT_EQ (panel::ledAt ({ 1, 4 }), -1);
}

// The permutation A³ Motion's adapter writes by (hwIndexToLedId): the same
// LED for every key, derived here from the places instead of copied.
TEST (PanelProtocol, LedsMatchTheMotionAdapter)
{
	const int motion[44] = { 39, 40, 41, 38, 34, 37, 35, 36, 31, 32, 33, 30, 26, 29, 27, 28,
							 23, 24, 25, 22, 18, 21, 19, 20, 15, 16, 17, 14, 10, 13, 11, 12,
							 7,  8,  9,  6,  2,  5,  3,  4, 43, 42,  0,  1 };
	for (int i = 0; i < panel::buttonCount; ++i)
		EXPECT_EQ (panel::ledAt (panel::buttonCell (i)), motion[i]) << "button " << i;
}

TEST (PanelProtocol, PlacesOfAFewKeys)
{
	EXPECT_EQ (panel::buttonCell (0), (Cell { 4, 0 }));
	EXPECT_EQ (panel::buttonCell (4), (Cell { 2, 1 }));
	EXPECT_EQ (panel::buttonCell (40), (Cell { 0, 0 }));
	EXPECT_EQ (panel::buttonCell (43), (Cell { 1, 9 }));
}
