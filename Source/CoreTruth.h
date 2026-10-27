// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include "TruthKeeperLink.h"

#include <memory>
#include <string>

// The one truth as both screens take it (spec truth-from-core).
namespace coretruth
{
	// Chosen once and hashed at once: the keeper's own fingerprint must be of
	// this truth, not of a cache Motion may write a moment later.
	struct Chosen
	{
		std::string path;
		juce::String hash;
	};
	Chosen choose();

	// Following Core's truth: a keeper that restarts StemDeck into a new one.
	// Null with $A3_OSC_TRUTH set: that file wins at every start, and
	// following would restart StemDeck into it forever.
	std::unique_ptr<TruthKeeperLink> follow (const Chosen&);
}
