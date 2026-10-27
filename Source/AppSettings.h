// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <juce_data_structures/juce_data_structures.h>

// Where StemDeck keeps its settings (~/.config/StemDeck/StemDeck.settings)
// and, beside them, the analysis cache. One place for both screens: a
// -bformat instance shares the file with the two-deck app.
namespace appsettings
{
	juce::PropertiesFile::Options options();
	juce::File analysisCacheFile();
	// Which StemDeck instance holds the A3 Motion panel: the deck letter (PanelOwnership).
	// Under $XDG_RUNTIME_DIR, so a reboot clears it; ~/.config would keep a stale one.
	juce::File panelLockFile();

	// -bformat: what is one instance's own (its panel choice, window place,
	// phones) lives in ~/.config/StemDeck/<X>/StemDeck.settings, beside its session. A
	// PropertiesFile saves the whole file, so two instances sharing one would
	// drop each other's keys.
	juce::File instanceSettingsFile (const juce::File& configDir, char letter);
	juce::PropertiesFile::Options instanceOptions (const juce::File& configDir, char letter);
	juce::PropertiesFile::Options instanceOptions (char letter);
}
