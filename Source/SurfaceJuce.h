#pragma once

#include <JuceHeader.h>
#include "SurfaceLayout.h"

// Between the pure surface layout (SurfaceLayout.h) and JUCE's rectangles.
namespace surface
{
	inline juce::Rectangle<int> toJuce (const Rect& r) { return { r.x, r.y, r.w, r.h }; }
	inline Rect fromJuce (const juce::Rectangle<int>& r) { return { r.getX(), r.getY(), r.getWidth(), r.getHeight() }; }
}
