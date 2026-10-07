#include "MeterBallistics.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float fullScale = 1.0f;
	constexpr float clipHoldSeconds = 1.0f;

	float toDb (float gain)
	{
		return gain > 0.0f ? std::max (PeakMeter::floorDb, 20.0f * std::log10 (gain)) : PeakMeter::floorDb;
	}

	float toGain (float db)
	{
		return db > PeakMeter::floorDb ? std::pow (10.0f, db / 20.0f) : 0.0f;
	}
}

void PeakMeter::feed (float peak, float seconds)
{
	const auto peakDb = toDb (peak);
	const auto fall = parameters.releaseDbPerSecond * seconds;

	const auto holding = std::min (seconds, holdRemaining);
	holdRemaining -= holding;
	holdDb = std::max (floorDb, holdDb - parameters.releaseDbPerSecond * (seconds - holding));

	levelDb = peakDb > levelDb ? risenTowards (peakDb, seconds)
							   : std::max ({ floorDb, levelDb - fall, peakDb });

	if (levelDb >= holdDb)
	{
		holdDb = levelDb;
		holdRemaining = parameters.peakHoldSeconds;
	}
}

float PeakMeter::risenTowards (float peakDb, float seconds) const
{
	if (parameters.attackMs <= 0.0f)
		return peakDb;

	const auto share = 1.0f - std::exp (-seconds * 1000.0f / parameters.attackMs);
	return levelDb + (peakDb - levelDb) * share;
}

float PeakMeter::level() const { return toGain (levelDb); }
float PeakMeter::hold() const { return toGain (holdDb); }

int segmentsLit (float gain, int segments)
{
	const auto share = (toDb (gain) - PeakMeter::floorDb) / -PeakMeter::floorDb;
	return std::clamp ((int) std::lround (share * (float) segments), 0, segments);
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
