#include "MeterBallistics.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace
{
	constexpr float fullScale = 1.0f;
	constexpr float clipHoldSeconds = 1.0f;

	float toDb (float gain)
	{
		return gain > 0.0f ? std::max (PeakMeter::floorDb, 20.0f * std::log10 (gain)) : PeakMeter::floorDb;
	}

	constexpr float ledThresholdsDb[] { -36.0f, -24.0f, -18.0f, -12.0f, -9.0f, -6.0f, -3.0f, 0.0f };
	constexpr int numLeds = (int) std::size (ledThresholdsDb);
	static_assert (meterScaleFloorDb == 2.0f * ledThresholdsDb[0] - ledThresholdsDb[1],
				   "the floor continues the first LED step's slope, as on the desk");

	constexpr int firstYellowLed = 4, firstRedLed = 6; // zero-based: -9 and -3
}

void PeakMeter::feed (float peak, float seconds)
{
	const auto peakDb = toDb (peak);
	const auto fall = parameters.releaseDbPerSecond * seconds;

	const auto holding = std::min (seconds, holdRemaining);
	holdRemaining -= holding;
	heldDb = std::max (floorDb, heldDb - parameters.releaseDbPerSecond * (seconds - holding));

	shownDb = peakDb > shownDb ? risenTowards (peakDb, seconds)
							   : std::max ({ floorDb, shownDb - fall, peakDb });

	if (shownDb >= heldDb)
	{
		heldDb = shownDb;
		holdRemaining = parameters.peakHoldSeconds;
	}
}

float PeakMeter::risenTowards (float peakDb, float seconds) const
{
	if (parameters.attackMs <= 0.0f)
		return peakDb;

	const auto share = 1.0f - std::exp (-seconds * 1000.0f / parameters.attackMs);
	return shownDb + (peakDb - shownDb) * share;
}

float barFraction (float db)
{
	if (db <= meterScaleFloorDb)
		return 0.0f;

	auto lowerDb = meterScaleFloorDb;
	for (int led = 0; led < numLeds; ++led)
	{
		const auto upperDb = ledThresholdsDb[led];
		if (db <= upperDb)
			return ((float) led + (db - lowerDb) / (upperDb - lowerDb)) / (float) numLeds;
		lowerDb = upperDb;
	}
	return 1.0f;
}

int segmentsLit (float db, int segments)
{
	// The epsilon keeps a level exactly on a segment's top from rounding below it.
	const auto lit = (int) std::floor (barFraction (db) * (float) segments + 1.0e-4f);
	return std::clamp (lit, 0, segments);
}

MeterZone zoneOfSegment (int index, int segments)
{
	// The segment's top, (index + 1) / segments, against the LEDs' eighths.
	const auto topTimesLeds = (index + 1) * numLeds;
	if (topTimesLeds > firstRedLed * segments)
		return MeterZone::red;
	if (topTimesLeds > firstYellowLed * segments)
		return MeterZone::yellow;
	return MeterZone::green;
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
