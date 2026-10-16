#include "CueLight.h"

#include <cmath>

namespace cuelight
{
	CueLight cueLight (bool loaded, bool playing, bool held, double position, double cuePoint)
	{
		if (! loaded)
			return CueLight::off;

		if (held)
			return CueLight::steady;

		if (playing)
			return CueLight::off;

		return std::abs (position - cuePoint) <= onCueTolerance ? CueLight::steady : CueLight::blink;
	}

	bool blinkPhaseOn (double seconds)
	{
		return std::fmod (std::floor (seconds / blinkHalfPeriod), 2.0) == 0.0;
	}
}
