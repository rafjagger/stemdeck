#pragma once

#include <JuceHeader.h>
#include "Sections.h"
#include "StemSet.h"
#include "TempoAnalysis.h"

#include <functional>
#include <optional>

// The section features of a set (Sections.h), measured from its four stems
// on the beats of `grid`. Reads every stem once, decimated to ~11 kHz as
// TempoAnalysis does; runs for a few seconds, so call it off the message
// thread. Nothing if aborted, a stem does not open, the stems' sample
// rates differ, or there is no grid.
namespace SectionAnalysis
{
	std::optional<sections::Features> analyse (const StemSet& set, juce::AudioFormatManager& formatManager,
											   const BeatGrid& grid,
											   const std::function<bool()>& shouldAbort = [] { return false; });
}
