// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

// What the command line asks for. -bformat (or --bformat) starts one deck per
// instance; without it StemDeck is the two-deck app it always was.
struct LaunchOptions
{
	bool oneDeck = false;
	char deck = 'A';
	bool fullscreen = false;
	double pathLatencyMs = 0.0;   // STEMDECK_PATH_LATENCY_MS: audio in flight between read head and ears
};

// argv without the program name. Words are compared whole; the error text is
// for stderr. `pathLatencyEnv` is STEMDECK_PATH_LATENCY_MS when set: 0...200,
// anything else is refused rather than guessed.
std::variant<LaunchOptions, std::string> parseLaunch (const std::vector<std::string>& argv,
                                                       const std::optional<std::string>& pathLatencyEnv = std::nullopt);
