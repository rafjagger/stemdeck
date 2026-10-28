// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "PotPickup.h"
#include "StemColumn.h"

#include <array>
#include <functional>
#include <optional>

// The panel's four pots as the stems' AUX knobs (R13): each goes through a
// PotPickup, and the knob shows the pot's place until it has picked up.
// Message thread only.
class AuxPots
{
public:
	static constexpr int count = 4;
	explicit AuxPots (std::function<StemColumn&(int)> columnOf) : column (std::move (columnOf)) {}

	// A pot's reading, 0..1.
	void reading (int stem, double value);
	// Every knob's value was replaced (session, set, restore) or the panel was gained.
	void rearm();
	// Once a pot has picked up, a touch on the knob or a load moving the value
	// away from it re-arms that pot, so its next move cannot jump the value.
	void watch();

private:
	void rearm (int stem);
	void showGhost (int stem);

	std::function<StemColumn&(int)> column;
	std::array<PotPickup, count> pickups;
	std::array<std::optional<double>, count> lastReading;   // survives a rearm: the ghost shows at once
	std::array<double, count> applied {};                   // what the pot last wrote to its knob
};
