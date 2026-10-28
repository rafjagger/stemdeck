// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "LaunchOptions.h"

#include <cctype>
#include <cstdlib>

namespace
{
	// R11: the truth's {deck} range is [1, 2] until it grows.
	constexpr char lastDeckLetter = 'B';
	constexpr double longestPathLatencyMs = 200.0;

	std::optional<double> parsePathLatencyMs (const std::string& text)
	{
		if (text.empty())
			return std::nullopt;
		char* end = nullptr;
		const auto value = std::strtod (text.c_str(), &end);
		if (*end != '\0' || ! (value >= 0.0 && value <= longestPathLatencyMs))   // also catches nan
			return std::nullopt;
		return value;
	}
}

std::variant<LaunchOptions, std::string> parseLaunch (const std::vector<std::string>& argv,
                                                       const std::optional<std::string>& pathLatencyEnv)
{
	LaunchOptions options;
	bool deckGiven = false;

	for (size_t i = 0; i < argv.size(); ++i)
	{
		const auto& word = argv[i];

		if (word == "-bformat" || word == "--bformat")
			options.oneDeck = true;
		else if (word == "--fullscreen")
			options.fullscreen = true;
		else if (word == "--deck")
		{
			if (i + 1 >= argv.size())
				return std::string ("--deck needs a letter (A or B)");

			const auto& value = argv[++i];
			const char letter = value.size() == 1
				? static_cast<char> (std::toupper (static_cast<unsigned char> (value[0])))
				: '\0';

			if (letter < 'A' || letter > lastDeckLetter)
				return "--deck " + value + " is not a deck: use A or B";

			options.deck = letter;
			deckGiven = true;
		}
		else
			return "unknown option " + word;
	}

	if (deckGiven && ! options.oneDeck)
		return std::string ("--deck only makes sense with -bformat");

	if (pathLatencyEnv)
	{
		const auto latency = parsePathLatencyMs (*pathLatencyEnv);
		if (! latency)
			return "STEMDECK_PATH_LATENCY_MS=" + *pathLatencyEnv + " is not 0 to 200 (milliseconds)";
		options.pathLatencyMs = *latency;
	}

	return options;
}
