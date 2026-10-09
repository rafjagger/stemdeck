#include "PanelProtocol.h"

namespace panel
{
	namespace
	{
		// The keys as "RC" (row, column) in the firmware's read order
		// (multiplexer_map.h's MATRIX_BUTTONS[], host.py's BUTTON_LABELS) ...
		constexpr char buttonLabels[buttonCount][3] = {
			"40", "30", "20", "50", "21", "51", "31", "41",
			"42", "32", "22", "52", "23", "53", "33", "43",
			"44", "34", "24", "54", "25", "55", "35", "45",
			"46", "36", "26", "56", "27", "57", "37", "47",
			"48", "38", "28", "58", "29", "59", "39", "49",
			"00", "10", "09", "19",
		};

		// ... and in the LED chain's order (LED_MAP[], host.py's
		// LED_BUTTON_LABELS): the chain snakes through the panel in an order
		// of its own, so the two meet only by place.
		constexpr char ledLabels[ledCount][3] = {
			"09", "19", "29", "39", "49", "59",
			"58", "48", "38", "28",
			"27", "37", "47", "57",
			"56", "46", "36", "26",
			"25", "35", "45", "55",
			"54", "44", "34", "24",
			"23", "33", "43", "53",
			"52", "42", "32", "22",
			"21", "31", "41", "51",
			"50", "40", "30", "20", "10", "00",
		};

		constexpr Cell cellOfLabel (const char* label) { return { label[0] - '0', label[1] - '0' }; }

		constexpr std::size_t buttonFrameSize = 12, encoderFrameSize = 25, potFrameSize = 9;

		int u16 (const std::uint8_t* p) { return p[0] | (p[1] << 8); }

		void readButtons (const std::uint8_t* p, Frame& frame)
		{
			for (int i = 0; i < buttonCount; ++i)
				frame.buttons[(size_t) i] = (KeyState) ((p[1 + i / 4] >> ((i % 4) * 2)) & 0x03);
		}

		void readEncoders (const std::uint8_t* p, Frame& frame)
		{
			for (int i = 0; i < encoderCount; ++i)
			{
				const auto* e = p + 1 + i * 3;
				frame.encoders[(size_t) i].delta = (std::int16_t) (std::uint16_t) u16 (e);
				frame.encoders[(size_t) i].key = (KeyState) (e[2] & 0x03);
			}
		}
	}

	bool isKey (Cell cell)
	{
		if (cell.row < 0 || cell.row >= rows || cell.col < 0 || cell.col >= columns)
			return false;
		return cell.col == 0 || cell.col == columns - 1 || cell.row >= firstPadRow;
	}

	Cell buttonCell (int button)
	{
		if (button < 0 || button >= buttonCount)
			return { -1, -1 };
		return cellOfLabel (buttonLabels[button]);
	}

	int ledAt (Cell cell)
	{
		for (int led = 0; led < ledCount; ++led)
			if (cellOfLabel (ledLabels[led]) == cell)
				return led;
		return -1;
	}

	std::vector<std::uint8_t> pollRequest (bool withPots)
	{
		if (withPots)
			return { getButtons, getEncoders, getPots };
		return { getButtons, getEncoders };
	}

	std::size_t pollReplySize (bool withPots)
	{
		return buttonFrameSize + encoderFrameSize + (withPots ? potFrameSize : 0);
	}

	std::optional<Frame> parsePollReply (const std::uint8_t* data, std::size_t size, bool withPots)
	{
		if (data == nullptr || size < pollReplySize (withPots))
			return std::nullopt;

		std::size_t buttons = 0, encoders = 0;
		if (data[0] == getButtons && data[buttonFrameSize] == getEncoders)
			encoders = buttonFrameSize;
		else if (data[0] == getEncoders && data[encoderFrameSize] == getButtons)
			buttons = encoderFrameSize;
		else
			return std::nullopt;

		const auto pots = buttonFrameSize + encoderFrameSize;
		if (withPots && data[pots] != getPots)
			return std::nullopt;

		Frame frame;
		readButtons (data + buttons, frame);
		readEncoders (data + encoders, frame);
		if (withPots)
		{
			std::array<int, potCount> values {};
			for (int i = 0; i < potCount; ++i)
				values[(size_t) i] = u16 (data + pots + 1 + i * 2);
			frame.pots = values;
		}
		return frame;
	}

	std::array<std::uint8_t, 5> setLed (int led, Rgb colour)
	{
		return { setLedCommand, (std::uint8_t) led, colour.r, colour.g, colour.b };
	}
}
