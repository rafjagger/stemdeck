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
