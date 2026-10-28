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

	// Where a deck's spatial ports lie among its own channels; aux -1: not rendered.
	// binauralCue: the cue at `cue` is the binaural pair (two-deck, ruling R2);
	// otherwise it is the 9-channel B-format cue and `aux` the 16-channel AUX bus.
	struct SpatialLayout { int channels, bformat, cue, aux, phones; bool binauralCue; };
	inline SpatialLayout twoDeckSpatialLayout()
	{
		return { spatialChannelsPerDeck(), bformatOffset, binauralCueOffset, -1, phonesOffset, true };
	}

	// A -bformat instance (spec M1/P10): one deck, its own JACK client
	// StemDeck-<X>, and only these 43 -- no bus ports. The cue leaves as
	// 2nd-order B-format (9 channels) so that Core decodes it once.
	namespace oneDeck
	{
		constexpr int auxChannels = bformatChannels;
		constexpr int cueChannels = 9;
		constexpr int channels = bformatChannels + auxChannels + cueChannels + 2;   // B-format, AUX, cue, phones
		inline SpatialLayout layout()
		{
			return { channels, 0, bformatChannels + auxChannels, bformatChannels, bformatChannels + auxChannels + cueChannels, false };
		}
		inline int channelCount() { return layout().channels; }

		inline std::string portName (int channel)
		{
			const auto l = layout();
			if (channel < l.aux)
				return "bformat_" + std::to_string (channel - l.bformat + 1);
			if (channel < l.cue)
				return "aux_" + std::to_string (channel - l.aux + 1);
			if (channel < l.phones)
				return "cue_" + std::to_string (channel - l.cue + 1);
			return channel == l.phones ? "phones_L" : "phones_R";
		}

		// Without JACK: every channel to its own output when the device has
		// them all; on a smaller device (a laptop's stereo) only the phones,
		// onto its first pair -- B-format summed to stereo is nothing to hear.
		inline int foldsInto (int channel, int numDeviceOutputs)
		{
			if (numDeviceOutputs >= channelCount())
				return channel;
			const auto phones = layout().phones;
			if (numDeviceOutputs >= 2 && (channel == phones || channel == phones + 1))
				return channel - phones;
			return -1;
		}
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
