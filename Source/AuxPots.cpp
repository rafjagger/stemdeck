// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "AuxPots.h"

void AuxPots::reading (int stem, double value)
{
	if (stem < 0 || stem >= count)
		return;
	column (stem).aux().setValue (value, juce::sendNotificationSync);
}
