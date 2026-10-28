// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "LaunchOptions.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace
{
	const LaunchOptions& ok (const std::variant<LaunchOptions, std::string>& r)
	{
		EXPECT_TRUE (std::holds_alternative<LaunchOptions> (r));
		static const LaunchOptions fallback;
		return std::holds_alternative<LaunchOptions> (r) ? std::get<LaunchOptions> (r) : fallback;
	}

	bool refused (const std::vector<std::string>& argv)
	{
		const auto r = parseLaunch (argv);
		return std::holds_alternative<std::string> (r) && ! std::get<std::string> (r).empty();
	}
}

TEST (LaunchOptions, SingleDashBformat)
{
	const auto r = parseLaunch ({ "-bformat" });
	EXPECT_TRUE (ok (r).oneDeck);
	EXPECT_EQ (ok (r).deck, 'A');
}

TEST (LaunchOptions, DoubleDashBformat)
{
	EXPECT_TRUE (ok (parseLaunch ({ "--bformat" })).oneDeck);
}

TEST (LaunchOptions, NoFlagIsTwoDeck)
{
	const auto r = parseLaunch ({});
	EXPECT_FALSE (ok (r).oneDeck);
	EXPECT_FALSE (ok (r).fullscreen);
}

TEST (LaunchOptions, DeckDefaultsToA)
{
	EXPECT_EQ (ok (parseLaunch ({ "--bformat" })).deck, 'A');
}

TEST (LaunchOptions, DeckLetterIsUpperCased)
{
	EXPECT_EQ (ok (parseLaunch ({ "--bformat", "--deck", "b" })).deck, 'B');
	EXPECT_EQ (ok (parseLaunch ({ "--bformat", "--deck", "A" })).deck, 'A');
}

TEST (LaunchOptions, DeckWithoutBformatIsRefused)
{
	EXPECT_TRUE (refused ({ "--deck", "A" }));
}

TEST (LaunchOptions, DeckPastBIsRefused)
{
	EXPECT_TRUE (refused ({ "--bformat", "--deck", "C" }));
	EXPECT_TRUE (refused ({ "--bformat", "--deck", "c" }));
	EXPECT_TRUE (refused ({ "--bformat", "--deck", "AB" }));
	EXPECT_TRUE (refused ({ "--bformat", "--deck" }));
	EXPECT_TRUE (refused ({ "--bformat", "--deck", "1" }));
}

TEST (LaunchOptions, UnknownFlagIsRefused)
{
	EXPECT_TRUE (refused ({ "-x" }));
	EXPECT_TRUE (refused ({ "--foo" }));
	EXPECT_TRUE (refused ({ "--bformat", "--foo" }));
}

TEST (LaunchOptions, FullscreenIsKept)
{
	EXPECT_TRUE (ok (parseLaunch ({ "--fullscreen" })).fullscreen);
	const auto r = parseLaunch ({ "--bformat", "--fullscreen", "--deck", "B" });
	EXPECT_TRUE (ok (r).fullscreen);
	EXPECT_TRUE (ok (r).oneDeck);
	EXPECT_EQ (ok (r).deck, 'B');
}

TEST (LaunchOptions, DashBformatIsNotABundleOfShortOptions)
{
	// Compared word by word: no prefix, no per-letter reading.
	EXPECT_TRUE (refused ({ "-bformatx" }));
	EXPECT_TRUE (refused ({ "-b" }));
	EXPECT_TRUE (refused ({ "-bf" }));
	EXPECT_TRUE (refused ({ "-fullscreen" }));
}

TEST (LaunchOptions, PathLatencyFromEnvironment)
{
	EXPECT_DOUBLE_EQ (ok (parseLaunch ({}, std::string ("18"))).pathLatencyMs, 18.0);
	EXPECT_DOUBLE_EQ (ok (parseLaunch ({}, std::string ("0"))).pathLatencyMs, 0.0);
	EXPECT_DOUBLE_EQ (ok (parseLaunch ({}, std::string ("200"))).pathLatencyMs, 200.0);
	EXPECT_DOUBLE_EQ (ok (parseLaunch ({}, std::string ("18.5"))).pathLatencyMs, 18.5);
	EXPECT_DOUBLE_EQ (ok (parseLaunch ({}, std::nullopt)).pathLatencyMs, 0.0) << "unset: no offset";

	for (const char* bad : { "-1", "x", "500", "", "18ms", "nan" })
	{
		const auto r = parseLaunch ({}, std::string (bad));
		ASSERT_TRUE (std::holds_alternative<std::string> (r)) << bad;
		EXPECT_NE (std::get<std::string> (r).find ("STEMDECK_PATH_LATENCY_MS"), std::string::npos) << bad;
	}
}
