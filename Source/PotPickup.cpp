// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "PotPickup.h"

#include <cmath>

void PotPickup::rearm (double storedValue)
{
	stored = storedValue;
	lastReading.reset();
	picked = false;
}

std::optional<double> PotPickup::feed (double potValue)
{
	const auto before = lastReading;
	lastReading = potValue;
	if (picked)
		return potValue;

	const auto close = std::abs (potValue - stored) <= nearEnough;
	const auto crossed = before.has_value() && (*before - stored) * (potValue - stored) <= 0.0;
	if (! close && ! crossed)
		return std::nullopt;

	picked = true;
	return potValue;
}
