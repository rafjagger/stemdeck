#include "MeterBallistics.h"

#include <algorithm>

namespace
{
	constexpr float releasePerTick = 0.85f;
	constexpr float silence = 0.001f; // -60 dB, the bottom of the meter scale
}

float nextMeterLevel (float shownLevel, float newPeak)
{
	const auto next = std::max (newPeak, shownLevel * releasePerTick);

	// The release is geometric and never reaches zero on its own.
	return next < silence ? 0.0f : next;
}
