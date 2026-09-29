#include "Scs3d.h"

#include <algorithm>
#include <cmath>

namespace scs3d
{
	namespace
	{
		constexpr std::uint8_t noteOn = 0x90, noteOff = 0x80, cc = 0xB0;
		constexpr std::uint8_t black = 0, red = 1, blue = 2, purple = 3;

		// Buttons
		constexpr std::uint8_t fx = 0x20, trig = 0x28, deckButton = 0x2A;
		constexpr std::uint8_t play = 0x6D, cue = 0x6E, sync = 0x6F, tap = 0x70;
		constexpr std::uint8_t circleTouch = 0x62;
		// Sliders: absolute CC at the id, relative (64 = still) at id + 1
		constexpr std::uint8_t gainAbs = 0x07, pitchRel = 0x04, circleRel = 0x63;
		// Meters: first id and count
		constexpr std::uint8_t gainMeter = 0x34, pitchMeter = 0x3F, circleMeter = 0x5D;
		constexpr int gainLights = 9, pitchLights = 9, circleLights = 16;
		constexpr std::uint8_t deckLightA = 0x71, deckLightB = 0x72, logo = 0x7A;

		// Meter light i (0: one end) as the device numbers it.
		std::uint8_t meterId (std::uint8_t first, int count, int i) { return (std::uint8_t) (first + count - 1 - i); }
	}

	Event decode (std::uint8_t status, std::uint8_t data1, std::uint8_t data2)
	{
		Event e;
		const auto kind = (std::uint8_t) (status & 0xF0);
		const bool press = kind == noteOn && data2 > 0;
		const bool release = kind == noteOff || (kind == noteOn && data2 == 0);

		if (press)
		{
			if (data1 >= fx && data1 <= fx + 6 && (data1 - fx) % 2 == 0)
				return { Event::Type::mute, (data1 - fx) / 2 };
			switch (data1)
			{
				case trig:        e.type = Event::Type::loopIn; break;
				case deckButton:  e.type = Event::Type::loopOut; break;
				case play:        e.type = Event::Type::play; break;
				case cue:         e.type = Event::Type::cueDown; break;
				case sync:        e.type = Event::Type::sync; break;
				case tap:         e.type = Event::Type::master; break;
				case circleTouch: e.type = Event::Type::scratchTouch; break;
				default: break;
			}
		}
		else if (release)
		{
			if (data1 == cue)         e.type = Event::Type::cueUp;
			if (data1 == circleTouch) e.type = Event::Type::scratchRelease;
		}
		else if (kind == cc)
		{
			switch (data1)
			{
				case gainAbs:   e = { Event::Type::gain, -1, data2 / 127.0 }; break;
				case pitchRel:  e = { Event::Type::pitch, -1, (double) data2 - 64.0 }; break;
				case circleRel: e = { Event::Type::scratchMove, -1, (double) data2 - 64.0 }; break;
				default: break;
			}
		}
		return e;
	}

	std::vector<Message> render (const Leds& leds)
	{
		std::vector<Message> out;
		const auto light = [&out] (std::uint8_t id, std::uint8_t value) { out.push_back ({ noteOn, id, value }); };

		light (logo, 1);
		light (deckLightA, leds.deck == 0);
		light (deckLightB, leds.deck == 1);

		for (int s = 0; s < 4; ++s)
			light ((std::uint8_t) (fx + 2 * s), leds.muted[(size_t) s] ? red : blue);

		light (trig, leds.looping || leds.loopInSet ? blue : black);
		light (deckButton, leds.looping ? red : black);
		light (play, leds.playing ? blue : black);
		light (cue, leds.atCue && ! leds.playing ? red : black);
		light (sync, leds.syncBent ? purple : leds.synced ? blue : black);
		light (tap, leds.master ? red : black);

		// Gain: a bar from the bottom.
		const auto gainTop = leds.gain <= 0.0 ? -1 : (int) std::lround (std::clamp (leds.gain, 0.0, 1.0) * (gainLights - 1));
		for (int i = 0; i < gainLights; ++i)
			light (meterId (gainMeter, gainLights, i), i <= gainTop);

		// Pitch: a bar from the middle towards the side it is off.
		const int centre = pitchLights / 2;
		const auto pitchPos = centre + (int) std::lround (std::clamp (leds.pitch, -1.0, 1.0) * centre);
		for (int i = 0; i < pitchLights; ++i)
			light (meterId (pitchMeter, pitchLights, i), (i >= std::min (centre, pitchPos) && i <= std::max (centre, pitchPos)));

		// Circle: one light going round with the platter.
		const auto needle = (int) std::floor (std::clamp (leds.platter - std::floor (leds.platter), 0.0, 0.999999) * circleLights);
		for (int i = 0; i < circleLights; ++i)
			light (meterId (circleMeter, circleLights, i), i == needle);

		return out;
	}

	std::vector<std::vector<std::uint8_t>> initSysex()
	{
		return { { 0xF0, 0x00, 0x01, 0x60, 0x02, 0x00, 0xF7 },    // MIDI channel 1
				 { 0xF0, 0x00, 0x01, 0x60, 0x01, 0x00, 0xF7 } };  // circle mode
	}

	std::vector<std::uint8_t> flatModeSysex()
	{
		return { 0xF0, 0x00, 0x01, 0x60, 0x10, 0x00, 0xF7 };
	}
}
