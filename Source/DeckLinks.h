// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <juce_core/juce_core.h>
#include <a3-links/LinkMap.h>

#include <optional>
#include <string>
#include <vector>

// Which JACK links a one-deck instance makes itself (plan ruling R8), in the
// engine's io format and with the engine's link code (a3::links).
namespace decklinks
{
	std::vector<std::string> ownOutputs();   // the 43 one-deck ports
	std::vector<std::string> ownInputs();    // rec_L, rec_R

	// P39: rec_L/R from the engine's REC bus (a3-engine:rec_L/R), the recorder's
	// links on the Core, where no STEMDECK_LINK_MAP is set.
	inline constexpr const char* installedCoreMap = "/usr/share/stemdeck/links-core.json";
}

// ~/.config/StemDeck/links-<X>.json; without it the file envMapFile names,
// with every "{X}" in a peer replaced by the letter; without either, the
// default map (decklinks::installedCoreMap), and with no default, no links.
// nullopt + error: a file that is named or there but wrong or missing.
std::optional<a3::links::LinkMap> deckLinkMap (char letter, const std::string& home, const char* envMapFile,
											   const juce::File& defaultMapFile, juce::String& error);

// The two-deck player's links: its rec_L/R from the default map, no links
// without one (a build that is not installed).
std::optional<a3::links::LinkMap> recorderLinkMap (const juce::File& defaultMapFile, juce::String& error);
