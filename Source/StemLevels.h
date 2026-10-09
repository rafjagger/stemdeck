#pragma once

#include <JuceHeader.h>
#include "StemHandover.h"
#include "StemSet.h"

#include <memory>

namespace StemLevels
{
	// Every stem's envelope (StemHandover.h), read through the whole set:
	// a disk read of the four files, so off the message and audio threads.
	// Null if a file cannot be read or `shouldAbort` says so.
	std::shared_ptr<const StemHandover::Envelopes> read (const StemSet& set, juce::AudioFormatManager& formatManager,
														 const std::function<bool()>& shouldAbort);
}
