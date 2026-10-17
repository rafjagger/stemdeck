#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "RemotePointer.h"

#include <optional>

namespace remote
{
	// A point given as a share of the screen picture (0,0 top-left), as a
	// point in the window; none when it falls outside the window.
	std::optional<juce::Point<float>> screenToWindow (juce::Point<float> share, juce::Rectangle<float> screen,
													  juce::Rectangle<float> window);

	// Feeds the inject to `topLevel`'s peer as touch source remoteTouchIndex.
	// Nothing when the component has no peer or the point is off the window.
	void injectPointer (juce::Component& topLevel, const Inject& inject, juce::int64 timeMs);
}
