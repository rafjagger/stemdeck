// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "AuxPots.h"

#include <cmath>

void AuxPots::reading (int stem, double value)
{
	if (stem < 0 || stem >= count)
		return;
	const auto s = (size_t) stem;
	auto& knob = column (stem).aux();
	lastReading[s] = value;
	watch();
	if (! pickups[s].pickedUp())
		pickups[s].setStored (knob.getValue());
	if (const auto picked = pickups[s].feed (value))
	{
		knob.setValue (*picked, juce::sendNotificationSync);
		applied[s] = knob.getValue();
	}
	showGhost (stem);
}

void AuxPots::rearm()
{
	for (int s = 0; s < count; ++s)
		rearm (s);
}

void AuxPots::panelGained()
{
	lastReading.fill (std::nullopt);
	rearm();
}

void AuxPots::watch()
{
	for (int s = 0; s < count; ++s)
		if (pickups[(size_t) s].pickedUp() && std::abs (column (s).aux().getValue() - applied[(size_t) s]) > 1e-6)
			rearm (s);
}

void AuxPots::rearm (int stem)
{
	const auto s = (size_t) stem;
	const auto stored = column (stem).aux().getValue();
	pickups[s].rearm (stored, lastReading[s]);
	applied[s] = stored;
	showGhost (stem);
}

void AuxPots::showGhost (int stem)
{
	const auto s = (size_t) stem;
	const auto show = lastReading[s] && ! pickups[s].pickedUp();
	column (stem).setAuxGhost (show ? std::optional<double> (pickups[s].ghost()) : std::nullopt);
}
