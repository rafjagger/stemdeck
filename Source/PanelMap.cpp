#include "PanelMap.h"

#include "PanelEdges.h"

#include <algorithm>
#include <cstdlib>

namespace panel
{
	namespace
	{
		constexpr int columnsPerDeck = columns / buses::decks;
		constexpr int cueRow = 0, playRow = 1;
		// The levels a transport key can be at: full while on, and at rest the
		// deck colour with its brightest channel at 214 of 255 --
		// the brightest channel of A3 Motion's idle key (buttonLeds.idle
		// r31 g214 b205). Rest and on then differ in colour, as on Motion.
		constexpr int restBrightest = 214;

		// A stem row's five slots in the screen row's order, deck B's left to
		// right; deck A is the same row from the right. The last is the dark
		// column, where the screen row has its knob.
		enum Slot { channelSlot, auxSlot, cueSlot, muteSlot, darkSlot, slotCount };
		static_assert (slotCount == columns / buses::decks);

		int slotOfColumn (int deck, int col)
		{
			const auto position = col - deck * (columns / buses::decks);
			return deck == 0 ? slotCount - 1 - position : position;
		}
		int columnOfSlot (int deck, int slot)
		{
			const auto position = deck == 0 ? slotCount - 1 - slot : slot;
			return deck * (columns / buses::decks) + position;
		}

		int deckOfColumn (int col) { return col / columnsPerDeck; }
		int outerColumn (int deck) { return deck == 0 ? 0 : columns - 1; }

		// Which slot of its row a pad control sits in; -1 for a route the stem
		// may not use.
		int slotOfControl (const Control& control)
		{
			switch (control.kind)
			{
				case Control::Kind::stemCue:  return cueSlot;
				case Control::Kind::stemMute: return muteSlot;
				case Control::Kind::route:
					if (! buses::mayUse (buses::stemIndex (control.deck, control.stem), control.bus))
						return -1;
					return control.bus == buses::aux ? auxSlot : channelSlot;
				case Control::Kind::none:
				case Control::Kind::cue:
				case Control::Kind::play: return -1;
			}
			return -1;
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
		c.stem = cell.row - firstPadRow;
		switch (slotOfColumn (c.deck, cell.col))
		{
			case channelSlot: c.kind = Control::Kind::route; c.bus = buses::channelOf (buses::stemIndex (c.deck, c.stem)); return c;
			case auxSlot:     c.kind = Control::Kind::route; c.bus = buses::aux; return c;
			case cueSlot:     c.kind = Control::Kind::stemCue; return c;
			case muteSlot:    c.kind = Control::Kind::stemMute; return c;
			default:          return {};
		}
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
			case Control::Kind::stemCue:
			case Control::Kind::stemMute:
			{
				if (control.stem < 0 || control.stem >= buses::stemsPerDeck)
					return { -1, -1 };
				const auto slot = slotOfControl (control);
				return slot < 0 ? Cell { -1, -1 } : Cell { firstPadRow + control.stem, columnOfSlot (control.deck, slot) };
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
			const auto e = tracker.key (i, frame.buttons[(size_t) i]);
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
				case Control::Kind::stemCue:  if (e.pressed) add (Event::Type::stemCue); break;
				case Control::Kind::stemMute: if (e.pressed) add (Event::Type::mute); break;
				case Control::Kind::none: break;
			}
		}

		for (int i = 0; i < encoderCount; ++i)
		{
			const auto& encoder = frame.encoders[(size_t) i];
			const auto knob = encoderStem (i);
			if (encoder.delta != 0)
				events.push_back ({ Event::Type::gain, knob.deck, knob.stem, -1, (double) encoder.delta });
			if (tracker.push (i, encoder.key).pressed)
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
		tracker.reset();
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

	Rgb scaled (Rgb c, int numerator, int denominator)
	{
		return { (std::uint8_t) (c.r * numerator / denominator),
				 (std::uint8_t) (c.g * numerator / denominator),
				 (std::uint8_t) (c.b * numerator / denominator) };
	}

	Rgb base (Rgb colour)
	{
		return scaled (vivid (colour), restBrightest, 255);
	}

	Leds compose (const LedState& state)
	{
		Leds leds {};

		for (int deck = 0; deck < buses::decks; ++deck)
		{
			// A stem's four pads are the screen's keys of its row: off in the
			// screen key's off colour, on in the key's own -- the stem's colour
			// on its channel, AUX's on AUX, cue's, mute's. A muted stem dims
			// nothing, as on the screen. CUE and PLAY rest in the deck's colour.
			const auto rest = base (state.deckColours[(size_t) deck]);
			for (int stem = 0; stem < buses::stemsPerDeck; ++stem)
			{
				const auto index = (size_t) buses::stemIndex (deck, stem);
				const auto pad = [&] (const Control& control, bool isOn, Rgb colour)
				{
					light (leds, control, isOn ? vivid (colour) : state.offColour);
				};
				for (int bus = 0; bus < buses::count; ++bus)
				{
					const auto isOn = (state.masks[index] & (1u << bus)) != 0;
					pad ({ Control::Kind::route, deck, stem, bus }, isOn, bus == buses::aux ? state.auxColour : state.stemColours[(size_t) stem]);
				}
				pad ({ Control::Kind::stemCue, deck, stem }, state.cued[index], state.cueColour);
				pad ({ Control::Kind::stemMute, deck, stem }, state.muted[index], state.muteColour);
			}

			const auto transport = [&] (Control::Kind kind, bool isOn, Rgb colour)
			{
				light (leds, { kind, deck }, isOn ? vivid (colour) : rest);
			};
			transport (Control::Kind::cue, state.atCue[(size_t) deck], state.cueColour);
			transport (Control::Kind::play, state.playing[(size_t) deck], state.playColour);
		}
		return leds;
	}
}
