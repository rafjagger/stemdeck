#pragma once

#include <string>

// StemDeck takes its truth from Core (spec truth-from-core, step 3), like the
// desk: it starts on the truth Core last served, hears /core/here, and on a
// fingerprint it does not have fetches, verifies, stores and restarts. The
// decisions are here, pure; the JUCE side is TruthKeeperLink.
//
// The port and the word below are all a device knows before it has a truth;
// a3-core's guard allows exactly these, by name.
namespace truthkeeper
{
	constexpr int announcePort = 7790;
	inline const char* announceAddress = "/core/here";

	inline std::string cachePath (const std::string& home)
	{
		return home + "/.cache/a3/a3-osc.json";
	}

	// radla (another subnet) never hears /core/here: there this file names
	// Core, and the keeper asks it every pollSeconds instead.
	constexpr int pollSeconds = 30;

	inline std::string corePath (const std::string& home)
	{
		return home + "/.config/a3/core";
	}

	// Core's address as the file gives it ("http://host:9080"), as the URL of
	// its truth; empty for a blank file, which names no Core.
	inline std::string pollUrl (const std::string& fileText)
	{
		const std::string blank = " \t\r\n";
		const auto first = fileText.find_first_not_of (blank);
		if (first == std::string::npos)
			return {};
		auto url = fileText.substr (first, fileText.find_last_not_of (blank) - first + 1);
		const std::string tail = "/api/truth";
		if (url.size() >= tail.size() && url.compare (url.size() - tail.size(), tail.size(), tail) == 0)
			return url;
		while (! url.empty() && url.back() == '/')
			url.pop_back();
		return url + tail;
	}

	// With $A3_OSC_TRUTH set, that file wins at every start: following Core
	// would restart StemDeck into the same file forever.
	inline bool followsCore (const char* override)
	{
		return override == nullptr || *override == '\0';
	}

	// Never fetch, never restart for the truth StemDeck already has.
	inline bool needsFetch (const std::string& announced, const std::string& own)
	{
		return announced != own;
	}

	inline bool verified (const std::string& bodyHash, const std::string& header, const std::string& announced)
	{
		return ! bodyHash.empty() && bodyHash == header && header == announced;
	}

	// $A3_OSC_TRUTH, else the cache if it reads as a truth, else the package's file.
	inline std::string startPath (const char* override, const std::string& cache, bool cacheUsable,
								  const std::string& package)
	{
		if (! followsCore (override))
			return override;
		return cacheUsable ? cache : package;
	}
}
