#pragma once

#include "Buses.h"

#include <string>

// What StemDeck puts on its outputs: the five stereo buses (Buses.h), mixed
// inside StemDeck and switched by remote control from the desk (spec
// stemdeck-remote, 2026-10-01; the 8x stereo mode is gone), then the cue pair
// cue_L / cue_R -- last, so the bus ports keep their names and order.
// Pure: no JUCE.
namespace outputs
{
	inline int busChannelCount() { return buses::count * 2; }
	inline int cueChannel() { return busChannelCount(); }
	inline int channelCount() { return cueChannel() + 2; }

	inline std::string portName (int channel)
	{
		const auto side = channel % 2 == 0 ? "_L" : "_R";
		return channel >= cueChannel() ? std::string ("cue") + side : buses::portName (channel / 2) + side;
	}
}
