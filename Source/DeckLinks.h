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
}

// ~/.config/StemDeck/links-<X>.json; without it the file envMapFile names,
// with every "{X}" in a peer replaced by the letter; without either, no links.
// nullopt + error: a file that is named but missing or wrong.
std::optional<a3::links::LinkMap> deckLinkMap (char letter, const std::string& home, const char* envMapFile, juce::String& error);
