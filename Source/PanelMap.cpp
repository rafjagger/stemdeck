#include "PanelMap.h"

#include <algorithm>
#include <cstdlib>

namespace panel
{
	namespace
	{
		constexpr int columnsPerDeck = columns / buses::decks;
		constexpr int cueRow = 0, playRow = 1;
		// Three levels a stem's pad can be at: full on its bus, a quarter on
		// its bus while muted, a twelfth -- the resting glow -- on the buses
		// it is not on; each well over a stop from the next. The twelfth is
		// also what keeps 32 resting keys and 10 lit ones inside the budget
		// (PanelMap test WorstCaseFitsTheBudgetWithoutScaling).
		constexpr int mutedShare = 4;
		constexpr int baseShare = 12;

		int deckOfColumn (int col) { return col / columnsPerDeck; }
		int outerColumn (int deck) { return deck == 0 ? 0 : columns - 1; }

		// A key's press and release from its two bits and whether it was
		// down at the last poll -- the firmware's states, read as A³ Motion
		// reads them.
		struct Edges { bool pressed = false, released = false; };

		Edges edges (KeyState state, bool& down)
		{
			Edges e;
			switch (state)
			{
				case KeyState::click:
					e.pressed = ! down;
					e.released = true;
					down = false;
					break;
				case KeyState::held:
					e.pressed = ! down;
					down = true;
					break;
				case KeyState::idle:
					e.released = down;
					down = false;
					break;
				case KeyState::bounce:
					break;
			}
			return e;
		}

		Rgb scaled (Rgb c, int numerator, int denominator)
		{
			return { (std::uint8_t) (c.r * numerator / denominator),
					 (std::uint8_t) (c.g * numerator / denominator),
					 (std::uint8_t) (c.b * numerator / denominator) };
		}

		void light (Leds& leds, const Control& control, Rgb colour)
		{
			const auto led = ledAt (cellOf (control));
			if (led >= 0)
				leds[(size_t) led] = colour;
		}
	}

	Control controlAt (Cell cell)
	{
		if (! isKey (cell))
			return {};

		Control c;
		c.deck = deckOfColumn (cell.col);
		if (cell.row < firstPadRow)
		{
			c.kind = cell.row == cueRow ? Control::Kind::cue : Control::Kind::play;
			return c;
		}
		c.kind = Control::Kind::route;
		c.stem = cell.row - firstPadRow;
		c.bus = buses::columnOrder (c.deck)[(size_t) (cell.col - c.deck * columnsPerDeck)];
		return c;
	}

	Cell cellOf (const Control& control)
	{
		if (control.deck < 0 || control.deck >= buses::decks)
			return { -1, -1 };

		switch (control.kind)
		{
			case Control::Kind::cue:  return { cueRow, outerColumn (control.deck) };
			case Control::Kind::play: return { playRow, outerColumn (control.deck) };
			case Control::Kind::route:
			{
				const auto order = buses::columnOrder (control.deck);
				const auto it = std::find (order.begin(), order.end(), control.bus);
				if (it == order.end() || control.stem < 0 || control.stem >= buses::stemsPerDeck)
					return { -1, -1 };
				return { firstPadRow + control.stem, control.deck * columnsPerDeck + (int) (it - order.begin()) };
			}
			case Control::Kind::none: break;
		}
		return { -1, -1 };
	}

	// The firmware counts the lower encoders first, a channel (two pad
	// columns) each, then the upper ones; the stems go in reading order.
	StemKnob encoderStem (int encoder)
	{
		if (encoder < 0 || encoder >= encoderCount)
			return {};
		const auto channel = encoder % 4;
		const auto upper = encoder >= 4;
		return { channel / 2, (upper ? 0 : 2) + channel % 2 };
	}

	// The pot nearer the middle: where the fader stands on the screen too.
	int faderPot (int deck)
	{
		return deck == 0 ? 1 : 2;
	}

	std::vector<Event> InputDecoder::feed (const Frame& frame)
	{
		std::vector<Event> events;

		for (int i = 0; i < buttonCount; ++i)
		{
			const auto e = edges (frame.buttons[(size_t) i], keyDown[(size_t) i]);
			if (! e.pressed && ! e.released)
				continue;

			const auto control = controlAt (buttonCell (i));
			const auto add = [&] (Event::Type type) { events.push_back ({ type, control.deck, control.stem, control.bus, 0.0 }); };
			switch (control.kind)
			{
				case Control::Kind::route: if (e.pressed) add (Event::Type::route); break;
				case Control::Kind::play:  if (e.pressed) add (Event::Type::play); break;
				case Control::Kind::cue:
					if (e.pressed)
						add (Event::Type::cueDown);
					if (e.released)
						add (Event::Type::cueUp);
					break;
				case Control::Kind::none: break;
			}
		}

		for (int i = 0; i < encoderCount; ++i)
		{
			const auto& encoder = frame.encoders[(size_t) i];
			const auto knob = encoderStem (i);
			if (encoder.delta != 0)
				events.push_back ({ Event::Type::gain, knob.deck, knob.stem, -1, (double) encoder.delta });
			if (edges (encoder.key, pushDown[(size_t) i]).pressed)
				events.push_back ({ Event::Type::mute, knob.deck, knob.stem, -1, 0.0 });
		}

		if (! frame.pots)
			return events;

		for (int pot = 0; pot < potCount; ++pot)
		{
			const auto raw = (*frame.pots)[(size_t) pot];
			auto& at = potAt[(size_t) pot];
			if (at && std::abs (raw - *at) <= potDeadband)
				continue;
			const auto known = at.has_value();
			at = raw;
			if (! known)
				continue;

			for (int deck = 0; deck < buses::decks; ++deck)
				if (faderPot (deck) == pot)
					events.push_back ({ Event::Type::fader, deck, -1, -1, std::clamp (raw / (double) potMaximum, 0.0, 1.0) });
		}
		return events;
	}

	void InputDecoder::reset()
	{
		keyDown = {};
		pushDown = {};
		potAt = {};
	}

	//==============================================================================
	Rgb vivid (Rgb c)
	{
		const int hi = std::max ({ c.r, c.g, c.b }), lo = std::min ({ c.r, c.g, c.b });
		if (hi == 0)
			return {};

		// HSV with V at full and S tripled; the hue is where each channel
		// stands between the lowest and the highest, which is kept.
		const auto saturation = std::min (1.0, 3.0 * (hi - lo) / hi);
		const auto newLo = 255.0 * (1.0 - saturation);
		const auto channel = [&] (int v)
		{
			const auto between = hi == lo ? 1.0 : (v - lo) / (double) (hi - lo);
			return (std::uint8_t) (newLo + between * (255.0 - newLo) + 0.5);
		};
		return { channel (c.r), channel (c.g), channel (c.b) };
	}

	Leds withinBudget (const Leds& leds)
	{
		int total = 0;
		for (const auto& c : leds)
			total += c.r + c.g + c.b;
		if (total <= budget)
			return leds;

		// Rounded down, every channel: the total cannot round up past the budget.
		Leds result;
		for (size_t i = 0; i < leds.size(); ++i)
			result[i] = scaled (leds[i], budget, total);
		return result;
	}

	Rgb base (Rgb colour)
	{
		return scaled (vivid (colour), 1, baseShare);
	}

	Leds render (const LedState& state) { return withinBudget (compose (state)); }

	Leds compose (const LedState& state)
	{
		Leds leds {};

		for (int deck = 0; deck < buses::decks; ++deck)
		{
			// Every key glows in what it is; what is on lights over it.
			for (int stem = 0; stem < buses::stemsPerDeck; ++stem)
			{
				const auto index = (size_t) buses::stemIndex (deck, stem);
				const auto stemColour = state.stemColours[(size_t) stem];
				auto on = vivid (stemColour);
				if (state.muted[index])
					on = scaled (on, 1, mutedShare);
				for (int bus = 0; bus < buses::count; ++bus)
				{
					const auto isOn = (state.masks[index] & (1u << bus)) != 0;
					light (leds, { Control::Kind::route, deck, stem, bus }, isOn ? on : base (stemColour));
				}
			}

			const auto transport = [&] (Control::Kind kind, bool isOn, Rgb colour)
			{
				light (leds, { kind, deck }, isOn ? vivid (colour) : base (colour));
			};
			transport (Control::Kind::cue, state.atCue[(size_t) deck], state.cueColour);
			transport (Control::Kind::play, state.playing[(size_t) deck], state.playColour);
		}
		return leds;
	}
}
