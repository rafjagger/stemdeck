// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Buses.h"

#include <array>
#include <optional>
#include <string>

// The laws of the deck's spatial engine, free of JUCE: Core's 3D law for
// stems, the deck-3D multiply, the steady bed and the phones weights.
namespace spatial
{
	constexpr int order = 3, bformatChannels = 16, stemsPerDeck = 4;
	constexpr float trim = buses::trim;   // on every deck output

	// Degrees; the default is the home pose.
	struct Pose { float azimuth = 0.0f, elevation = 0.0f, width = 60.0f; };

	// Core's moving-band gain for the effective 3D, with exact silence at 0
	// (below 1e-9): -40 dB just above 0 up to 0 dB at 1. NaN counts as 0.
	float movingGain (float effectiveThreeD);

	// clamp01 (deck) * max (clamp01 (stem), clamp01 (envelope)).
	float effectiveThreeD (float deckThreeD, float stemThreeD, float envelope);

	struct PhonesWeights { float cue, mix; };

	// level^2 times an equal-power CUE<->MIX; a far end is exactly 0.
	PhonesWeights phonesWeights (float level, float mix);

	struct BedSource { float azimuth; int side; };   // side 0 = L, 1 = R
	constexpr std::array<BedSource, 4> bedSources { { { 45.0f, 0 }, { -45.0f, 1 }, { -135.0f, 1 }, { 135.0f, 0 } } };
	constexpr float bedSourceGainDb = -6.0f;

	enum class PhonesMode { automatic, on, off };

	// StemDeck's own phones decoder is for standalone use: it runs only with
	// connected ports, and in automatic only while Core is not heard.
	constexpr bool phonesDecoderRuns (PhonesMode mode, bool portsConnected, bool coreHeard)
	{
		if (! portsConnected || mode == PhonesMode::off)
			return false;
		return mode == PhonesMode::on || ! coreHeard;
	}

	// Core is heard while its last /core/here (sent every 2 s) is at most
	// this old. Seconds on one monotonic clock.
	constexpr double coreHeardSeconds = 5.0;
	constexpr bool coreHeard (std::optional<double> lastHeardSeconds, double nowSeconds)
	{
		return lastHeardSeconds.has_value() && nowSeconds - *lastHeardSeconds <= coreHeardSeconds;
	}

	// The mode as the app settings store it ("automatic", "on", "off");
	// anything else is the default, automatic.
	std::string phonesModeName (PhonesMode mode);
	PhonesMode phonesModeFrom (const std::string& name);
}
