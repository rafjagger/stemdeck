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

	// Where an inject lands in the window, given whether the injected touch is
	// down. A down outside the window starts nothing; a move or up while down is
	// clamped to the window's edge so an up is never lost (a lost up would leave
	// the touch pressed for good); a move or up while not down is nothing.
	std::optional<juce::Point<float>> localPointFor (PointerEvent::Phase phase, bool touchDown, juce::Point<float> share,
													 juce::Rectangle<float> screen, juce::Rectangle<float> window);

	// Feeds the inject to `topLevel`'s peer as touch source remoteTouchIndex.
	// Nothing when the component has no peer; see localPointFor for the rest.
	// The one remote touch's down state is kept here (there is only touch#9).
	void injectPointer (juce::Component& topLevel, const Inject& inject, juce::int64 timeMs);
}
