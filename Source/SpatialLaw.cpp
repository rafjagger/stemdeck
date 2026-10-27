// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "SpatialLaw.h"

#include "a3-engine/Laws.h"

#include <cmath>

namespace spatial
{
	namespace
	{
		constexpr float silenceFloor = 1e-9f;
		constexpr double halfPi = 1.57079632679489661923;

		// NaN counts as 0: the values arrive unchecked.
		float clamp01 (float value)
		{
			if (! (value > 0.0f))
				return 0.0f;
			return value > 1.0f ? 1.0f : value;
		}

		float snapToSilence (double weight)
		{
			return weight < silenceFloor ? 0.0f : static_cast<float> (weight);
		}
	}

	float movingGain (float effectiveThreeD)
	{
		const float t = clamp01 (effectiveThreeD);
		if (t < silenceFloor)
			return 0.0f;
		const double decibels = a3::laws::bandGain (t) * 80.0 - 40.0;
		return static_cast<float> (std::pow (10.0, decibels / 20.0));
	}

	float effectiveThreeD (float deckThreeD, float stemThreeD, float envelope)
	{
		const float stem = clamp01 (stemThreeD), env = clamp01 (envelope);
		return clamp01 (deckThreeD) * (stem > env ? stem : env);
	}

	float auxGain (float aux)
	{
		return snapToSilence (clamp01 (aux));
	}

	PhonesWeights phonesWeights (float level, float mix)
	{
		const double level01 = clamp01 (level);
		const double levelGain = level01 * level01;
		const double angle = halfPi * clamp01 (mix);
		return { snapToSilence (levelGain * std::cos (angle)), snapToSilence (levelGain * std::sin (angle)) };
	}

	std::string phonesModeName (PhonesMode mode)
	{
		switch (mode)
		{
			case PhonesMode::on:  return "on";
			case PhonesMode::off: return "off";
			case PhonesMode::automatic: break;
		}
		return "automatic";
	}

	PhonesMode phonesModeFrom (const std::string& name)
	{
		if (name == "on")
			return PhonesMode::on;
		if (name == "off")
			return PhonesMode::off;
		return PhonesMode::automatic;
	}
}
