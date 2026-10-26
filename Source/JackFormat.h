#pragma once

#include <cstdint>

// When JackOutput may prepare its source. PipeWire's libjack calls the
// sample-rate callback from inside jack_set_sample_rate_callback, before the
// block size is known; a block of 0 divides by zero in JUCE's convolution.
// Pure: no JUCE, no JACK.
namespace jackformat
{
	inline bool complete (std::uint32_t bufferSize, double sampleRate)
	{
		return bufferSize > 0 && sampleRate > 0.0;
	}
}
