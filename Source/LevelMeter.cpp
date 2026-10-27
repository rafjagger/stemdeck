// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "LevelMeter.h"

namespace
{
	const auto clipColour = juce::Colour (0xffe04848);
}

float LevelMeter::secondsSinceLastFeed()
{
	// The meter's own clock: the main timer's 60 Hz is a wish, not a promise,
	// and the fall is 20 dB a second, not per so many ticks.
	const auto now = juce::Time::getMillisecondCounterHiRes();
	const auto seconds = lastFeedMs > 0.0 ? (now - lastFeedMs) / 1000.0 : 0.0;
	lastFeedMs = now;
	return (float) seconds;
}

void LevelMeter::setLevel (float newPeak)
{
	const auto seconds = secondsSinceLastFeed();
	ballistics.feed (newPeak, seconds);
	const auto nextClipping = showsClip && clipHold.feed (newPeak, seconds);

	const auto numSegments = meterSegments ((float) getHeight());
	const auto barSegments = showsClip ? numSegments - 1 : numSegments;
	const auto nextLit = segmentsLit (ballistics.levelDb(), barSegments);
	const auto nextHeld = segmentsLit (ballistics.holdDb(), barSegments);

	if (nextLit != litSegments || nextHeld != heldSegment || nextClipping != clipping)
	{
		litSegments = nextLit;
		heldSegment = nextHeld;
		clipping = nextClipping;
		repaint();
	}
}

void LevelMeter::paint (juce::Graphics& g)
{
	const auto bounds = getLocalBounds().toFloat();
	g.setColour (juce::Colours::black);
	g.fillRoundedRectangle (bounds, 2.0f);

	// Segmented LED bar on the desk's LED scale and in its LEDs' colours; with
	// a clip lamp, the top segment is the lamp and the bar ends at full scale
	// one below it.
	const int numSegments = meterSegments (bounds.getHeight());
	const int barSegments = showsClip ? numSegments - 1 : numSegments;
	const auto segmentHeight = bounds.getHeight() / (float) numSegments;

	const auto segmentAt = [&] (int i)
	{
		return juce::Rectangle<float> (bounds.getX() + 1.0f, bounds.getBottom() - segmentHeight * (float) (i + 1),
									   bounds.getWidth() - 2.0f, segmentHeight - 1.0f);
	};
	const auto colourOf = [this, barSegments] (int i)
	{
		switch (zoneOfSegment (i, barSegments))
		{
			case MeterZone::red:    return clipColour;
			case MeterZone::yellow: return juce::Colour (0xffe8c33d);
			case MeterZone::green:  return barColour.value_or (juce::Colour (0xff3ec46d));
		}
		return juce::Colour (0xff3ec46d);
	};

	for (int i = 0; i < barSegments; ++i)
	{
		g.setColour (i < litSegments ? colourOf (i) : colourOf (i).withAlpha (0.12f));
		g.fillRect (segmentAt (i));
	}

	// The held peak: a thin line, the top of its segment, above the bar.
	if (heldSegment > litSegments)
	{
		const auto held = heldSegment - 1;
		auto light = segmentAt (held);
		g.setColour (colourOf (held));
		g.fillRect (light.removeFromTop (juce::jmax (1.0f, light.getHeight() * 0.4f)));
	}

	if (! showsClip)
		return;

	g.setColour (clipping ? clipColour.brighter (0.4f) : clipColour.withAlpha (0.12f));
	g.fillRect (segmentAt (numSegments - 1));
}
