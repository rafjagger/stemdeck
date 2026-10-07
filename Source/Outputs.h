#pragma once

#include "Buses.h"

#include <string>

// What StemDeck puts on its outputs: the five stereo buses (Buses.h), mixed
// inside StemDeck and switched by remote control from the desk (spec
// stemdeck-remote, 2026-10-01; the 8x stereo mode is gone). Pure: no JUCE.
namespace outputs
{
	inline int channelCount() { return buses::count * 2; }

	inline std::string portName (int channel)
	{
		return buses::portName (channel / 2) + (channel % 2 == 0 ? "_L" : "_R");
	}
}
