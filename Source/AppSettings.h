// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

// Where StemDeck keeps its settings (~/.config/StemDeck/StemDeck.settings)
// and, beside them, the analysis cache. One place for both screens: a
// -bformat instance shares the file with the two-deck app.
namespace appsettings
{
	juce::PropertiesFile::Options options();
	juce::File analysisCacheFile();
	// Which StemDeck instance holds the A3 Motion panel: the deck letter (PanelOwnership).
	juce::File panelLockFile();
}
