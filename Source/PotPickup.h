// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>

// A pot that must not make a value jump (R13): it acts only after its reading
// has crossed the stored value, or come within one percent of full scale of
// it. Until then the screen shows where the pot stands (ghost). Pure.
class PotPickup
{
public:
	// A value was loaded or restored, or the panel was gained: the pot has to
	// find the value again.
	void rearm (double storedValue);
	// The stored value moved on its own (a touch on the knob) while the pot has not picked up.
	void setStored (double storedValue) { stored = storedValue; }
	// potValue 0..1. The value to apply once picked up, else nothing.
	std::optional<double> feed (double potValue);

	bool pickedUp() const { return picked; }
	bool hasReading() const { return lastReading.has_value(); }
	double ghost() const { return lastReading.value_or (stored); }

private:
	static constexpr double nearEnough = 0.01;

	double stored = 0.0;
	std::optional<double> lastReading;
	bool picked = false;
};
