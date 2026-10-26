#pragma once

#include "Buses.h"
#include "SpatialLaw.h"

#include <string>

// What StemDeck puts on its outputs: the five stereo buses (Buses.h), mixed
// inside StemDeck and switched by remote control from the desk (spec
// stemdeck-remote, 2026-10-01; the 8x stereo mode is gone), then the cue pair
// cue_L / cue_R -- last, so the bus ports keep their names and order.
// After these twelve, each deck's spatial ports (stage S2, ruling R7):
// 16 channels of B-format, a binaural cue pair and a phones pair.
// Pure: no JUCE.
namespace outputs
{
	inline int busChannelCount() { return buses::count * 2; }
	inline int cueChannel() { return busChannelCount(); }

	// Today's twelve: the five stereo buses and the cue pair.
	namespace stereo { inline int channelCount() { return cueChannel() + 2; } }

	constexpr int spatialDecks = 2;
	constexpr int bformatChannels = spatial::bformatChannels;
	inline int spatialChannelsPerDeck() { return bformatChannels + 2 + 2; }
	inline int spatialFirstChannel (int deck) { return stereo::channelCount() + spatialChannelsPerDeck() * deck; }
	inline int channelCount() { return spatialFirstChannel (spatialDecks); }

	// Within a deck's spatial ports.
	constexpr int bformatOffset = 0, binauralCueOffset = bformatChannels, phonesOffset = bformatChannels + 2;

	inline std::string spatialPortName (int channel)
	{
		const auto deck = (channel - stereo::channelCount()) / spatialChannelsPerDeck();
		const auto within = channel - spatialFirstChannel (deck);
		const auto prefix = std::string (1, (char) ('A' + deck)) + "_";
		const auto side = within % 2 == 0 ? "_L" : "_R";
		if (within < binauralCueOffset)
			return prefix + "bformat_" + std::to_string (within + 1);
		return prefix + (within < phonesOffset ? "bcue" : "phones") + side;
	}

	inline std::string portName (int channel)
	{
		if (channel >= stereo::channelCount())
			return spatialPortName (channel);
		const auto side = channel % 2 == 0 ? "_L" : "_R";
		return channel >= cueChannel() ? std::string ("cue") + side : buses::portName (channel / 2) + side;
	}

	// Without JACK, onto a device with numDeviceOutputs: the device output a
	// channel is added to, or -1. The buses are summed down onto fewer
	// outputs; the cue and the spatial channels only go to an output of their
	// own -- summed down they would land on a bus.
	inline int foldsInto (int channel, int numDeviceOutputs)
	{
		if (numDeviceOutputs <= 0)
			return -1;
		if (channel < cueChannel())
			return channel % numDeviceOutputs;
		return channel < numDeviceOutputs ? channel : -1;
	}
}
