#include "MeterBallistics.h"

#include <algorithm>

namespace
{
	constexpr float releasePerTick = 0.85f;
	constexpr float silence = 0.001f; // -60 dB, the bottom of the meter scale
	constexpr float fullScale = 1.0f;
	constexpr float clipHoldSeconds = 1.0f;
}

float nextMeterLevel (float shownLevel, float newPeak)
{
	const auto next = std::max (newPeak, shownLevel * releasePerTick);

	// The release is geometric and never reaches zero on its own.
	return next < silence ? 0.0f : next;
}

int meterSegments (float heightPixels)
{
	return std::clamp ((int) (heightPixels / 3.0f), 1, 24);
}

bool ClipHold::feed (float peak, float seconds)
{
	if (peak > fullScale)
	{
		remaining = clipHoldSeconds;
		return true;
	}

	remaining = std::max (0.0f, remaining - seconds);
	return remaining > 0.0f;
}
