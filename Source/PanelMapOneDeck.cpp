// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "PanelMapOneDeck.h"

#include <algorithm>
#include <cmath>

namespace panel1
{
	namespace
	{
		using Kind = Control::Kind;
		using motionlite::Shape;

		constexpr int stems = 4, endColumn = panel::columns - 1;
		constexpr int cueRow = 0, playRow = 1, shiftRow = 0;
		constexpr int loopRow = 2, loopInRow = 3, loopOutRow = 4, continueRow = 5;
		// The pad rows' two keys, left then right, row 2 downwards.
		constexpr std::array<std::array<Kind, 2>, 4> padRows { { { Kind::orbit, Kind::ping }, { Kind::rise, Kind::scat },
																 { Kind::stemCue, Kind::stemMute }, { Kind::lengthDown, Kind::lengthUp } } };
		constexpr int armedPeriodMs = 500;
		// The length keys at an end of their range: a quarter of the resting light.
		constexpr int dimNumerator = 1, dimDenominator = 4;

		bool isShot (Kind kind) { return kind == Kind::orbit || kind == Kind::ping || kind == Kind::rise || kind == Kind::scat; }

		Shape shapeOf (Kind kind)
		{
			switch (kind)
			{
				case Kind::ping: return Shape::ping;
				case Kind::rise: return Shape::rise;
				case Kind::scat: return Shape::scat;
				default:         return Shape::orbit;
			}
		}

		void light (panel::Leds& leds, const Control& control, Rgb colour)
		{
			const auto led = panel::ledAt (cellOf (control));
			if (led >= 0)
				leds[(size_t) led] = colour;
		}

		// The armed pad's blink: lit for the first half of each period.
		bool blinkOn (double nowSeconds)
		{
			const auto ms = (long long) std::floor (nowSeconds * 1000.0);
			return ((ms % armedPeriodMs) + armedPeriodMs) % armedPeriodMs < armedPeriodMs / 2;
		}
	}

	Control controlAt (Cell cell)
	{
		if (! panel::isKey (cell))
			return {};

		if (cell.col == 0 && cell.row == cueRow)
			return { Kind::cue, -1 };
		if (cell.col == 0 && cell.row == playRow)
			return { Kind::play, -1 };
		if (cell.col == 0 && cell.row == loopRow)
			return { Kind::loop, -1 };
		if (cell.col == 0 && cell.row == loopInRow)
			return { Kind::loopIn, -1 };
		if (cell.col == 0 && cell.row == loopOutRow)
			return { Kind::loopOut, -1 };
		if (cell.col == 0 && cell.row == continueRow)
			return { Kind::continuePlay, -1 };
		if (cell.col == endColumn && cell.row == shiftRow)
			return { Kind::shift, -1 };
		if (cell.col < 1 || cell.col > 2 * stems || cell.row < panel::firstPadRow)
			return {};

		const auto stem = (cell.col - 1) / 2, side = (cell.col - 1) % 2;
		return { padRows[(size_t) (cell.row - panel::firstPadRow)][(size_t) side], stem };
	}

	Cell cellOf (const Control& control)
	{
		switch (control.kind)
		{
			case Kind::cue:   return { cueRow, 0 };
			case Kind::play:  return { playRow, 0 };
			case Kind::loop:    return { loopRow, 0 };
			case Kind::loopIn:  return { loopInRow, 0 };
			case Kind::loopOut: return { loopOutRow, 0 };
			case Kind::continuePlay: return { continueRow, 0 };
			case Kind::shift: return { shiftRow, endColumn };
			case Kind::none:  return { -1, -1 };
			default: break;
		}
		if (control.stem < 0 || control.stem >= stems)
			return { -1, -1 };
		for (size_t row = 0; row < padRows.size(); ++row)
			for (size_t side = 0; side < 2; ++side)
				if (padRows[row][side] == control.kind)
					return { panel::firstPadRow + (int) row, 2 * control.stem + 1 + (int) side };
		return { -1, -1 };
	}

	StemKnob encoderStem (int encoder)
	{
		if (encoder < 0 || encoder >= panel::encoderCount)
			return {};
		return { encoder % stems, encoder < stems };
	}

	std::vector<Event> InputDecoder::feed (const panel::Frame& frame)
	{
		std::vector<Event> events;

		for (int i = 0; i < panel::buttonCount; ++i)
		{
			const auto e = tracker.key (i, frame.buttons[(size_t) i]);
			if (! e.pressed && ! e.released)
				continue;

			const auto control = panel1::controlAt (panel::buttonCell (i));
			const auto add = [&] (Event::Type type)
			{
				Event event;
				event.type = type;
				event.stem = control.stem;
				event.shape = shapeOf (control.kind);
				events.push_back (event);
			};
			if (isShot (control.kind))
			{
				if (e.pressed)
					add (Event::Type::shot);
				continue;
			}
			switch (control.kind)
			{
				case Kind::cue:
					if (e.pressed) add (Event::Type::cueDown);
					if (e.released) add (Event::Type::cueUp);
					break;
				case Kind::shift:
					if (e.pressed) add (Event::Type::shiftDown);
					if (e.released) add (Event::Type::shiftUp);
					break;
				case Kind::play:       if (e.pressed) add (Event::Type::play); break;
				case Kind::loop:       if (e.pressed) add (Event::Type::loop); break;
				case Kind::loopIn:     if (e.pressed) add (Event::Type::loopIn); break;
				case Kind::loopOut:    if (e.pressed) add (Event::Type::loopOut); break;
				case Kind::continuePlay: if (e.pressed) add (Event::Type::continuePlay); break;
				case Kind::stemCue:    if (e.pressed) add (Event::Type::stemCue); break;
				case Kind::stemMute:   if (e.pressed) add (Event::Type::stemMute); break;
				case Kind::lengthDown: if (e.pressed) add (Event::Type::lengthDown); break;
				case Kind::lengthUp:   if (e.pressed) add (Event::Type::lengthUp); break;
				default: break;
			}
		}

		for (int i = 0; i < panel::encoderCount; ++i)
		{
			const auto& encoder = frame.encoders[(size_t) i];
			const auto knob = encoderStem (i);
			if (encoder.delta != 0)
			{
				Event event;
				event.type = knob.width ? Event::Type::width : Event::Type::gain;
				event.stem = knob.stem;
				event.value = (double) encoder.delta;
				events.push_back (event);
			}
			// Only the upper encoder's push means something: MUTE, as on the two-deck panel.
			if (tracker.push (i, encoder.key).pressed && ! knob.width)
			{
				Event event;
				event.type = Event::Type::stemMute;
				event.stem = knob.stem;
				events.push_back (event);
			}
		}

		if (! frame.pots)
			return events;
		// The first reading of each pot is reported too: the screen shows where it stands.
		for (int pot = 0; pot < panel::potCount; ++pot)
		{
			const auto raw = (*frame.pots)[(size_t) pot];
			auto& at = potAt[(size_t) pot];
			if (at && std::abs (raw - *at) <= panel::InputDecoder::potDeadband)
				continue;
			at = raw;
			Event event;
			event.type = Event::Type::aux;
			event.stem = pot;
			event.value = std::clamp (raw / (double) panel::potMaximum, 0.0, 1.0);
			events.push_back (event);
		}
		return events;
	}

	//==============================================================================
	panel::Leds leds (const LedState& state, double nowSeconds)
	{
		panel::Leds out {};
		const auto rest = panel::base (state.deckColour);
		const auto lit = [] (Rgb colour) { return panel::vivid (colour); };

		light (out, { Kind::cue, -1 }, state.atCue ? lit (state.cueColour) : rest);
		light (out, { Kind::play, -1 }, state.playing ? lit (state.playColour) : rest);
		light (out, { Kind::shift, -1 }, state.shift ? lit (state.deckColour) : rest);
		const auto loopKey = [&] (Kind kind, bool on) { light (out, { kind, -1 }, on ? lit (state.loopColour) : rest); };
		loopKey (Kind::loop, state.looping);
		loopKey (Kind::loopIn, state.looping || state.loopInMarked);
		loopKey (Kind::loopOut, state.looping);
		light (out, { Kind::continuePlay, -1 }, state.continuing ? lit (state.playModeColour) : rest);

		for (int stem = 0; stem < stems; ++stem)
		{
			const auto& s = state.stems[(size_t) stem];
			const auto stemColour = lit (state.stemColours[(size_t) stem]);
			for (const auto kind : { Kind::orbit, Kind::ping, Kind::rise, Kind::scat })
			{
				const auto pad = s.pads[(size_t) shapeOf (kind)];
				const auto face = pad == motionlite::PadState::running ? stemColour
								  : pad == motionlite::PadState::armed && blinkOn (nowSeconds) ? stemColour
								  : rest;
				light (out, { kind, stem }, face);
			}
			light (out, { Kind::stemCue, stem }, s.cued ? lit (state.cueColour) : rest);
			light (out, { Kind::stemMute, stem }, s.muted ? lit (state.muteColour) : rest);
			const auto dim = panel::scaled (rest, dimNumerator, dimDenominator);
			light (out, { Kind::lengthDown, stem }, s.shortest ? dim : rest);
			light (out, { Kind::lengthUp, stem }, s.longest ? dim : rest);
		}
		return out;
	}
}
