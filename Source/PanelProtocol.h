#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

// The A³ Motion panel PCB as its firmware speaks over USB serial (a3-motion
// firmware/include/protocol.h; host.py says the same). Pure: no JUCE, no I/O
// -- the frames built and read here, the port is PanelDevice's.
//
// A request is one opcode byte; the answer starts with the same byte:
//   0x01 PING          -> [0x01]
//   0x02 GET_POTS      -> [0x02] 4 x u16 LE, 0..4095
//   0x03 GET_ENCODERS  -> [0x03] 8 x (i16 LE delta since the last read, u8 key state)
//   0x04 GET_BUTTONS   -> [0x04] 11 bytes, 44 keys x 2 bits, LSB first
//   0x05 SET_LED       <- [0x05] led r g b, no answer
//
// The panel is a grid of six rows and ten columns: a key in every row of the
// two end columns, the pads in rows 2-5 of the eight between them, the pots
// and encoders over the pads.
namespace panel
{
	constexpr std::uint8_t ping = 0x01, getPots = 0x02, getEncoders = 0x03, getButtons = 0x04, setLedCommand = 0x05;

	constexpr int rows = 6, columns = 10;
	constexpr int buttonCount = 44, ledCount = 44, encoderCount = 8, potCount = 4;
	constexpr int firstPadRow = 2;
	constexpr int potMaximum = 4095;

	struct Cell
	{
		int row = 0, col = 0;
		bool operator== (const Cell& o) const { return row == o.row && col == o.col; }
		bool operator!= (const Cell& o) const { return ! (*this == o); }
	};

	struct Rgb
	{
		std::uint8_t r = 0, g = 0, b = 0;
		bool operator== (const Rgb& o) const { return r == o.r && g == o.g && b == o.b; }
		bool operator!= (const Rgb& o) const { return ! (*this == o); }
	};

	// Whether the panel has a key there.
	bool isKey (Cell cell);

	// Where the firmware's key `button` (its read order, 0..43) sits.
	Cell buttonCell (int button);

	// The LED under the key at `cell` (the LED chain's own order), -1 for none.
	int ledAt (Cell cell);

	// A key's two bits, the same for the encoders' push: held down, up,
	// let go and pressed again within one poll, or pressed and let go.
	enum class KeyState : std::uint8_t { held = 0, idle = 1, bounce = 2, click = 3 };

	struct Encoder
	{
		int delta = 0;   // detents since the last poll, + clockwise
		KeyState key = KeyState::idle;
	};

	struct Frame
	{
		std::array<KeyState, buttonCount> buttons {};
		std::array<Encoder, encoderCount> encoders {};
		std::optional<std::array<int, potCount>> pots;
	};

	// One poll: buttons and encoders, the pots on the cycles that ask.
	std::vector<std::uint8_t> pollRequest (bool withPots);
	std::size_t pollReplySize (bool withPots);

	// The answer to pollRequest(), or nothing when the markers are not where
	// they belong (a stream that slipped). Buttons and encoders may arrive in
	// either order.
	std::optional<Frame> parsePollReply (const std::uint8_t* data, std::size_t size, bool withPots);

	std::array<std::uint8_t, 5> setLed (int led, Rgb colour);
}
