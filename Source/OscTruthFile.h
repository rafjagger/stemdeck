#pragma once

#include "OscTruth.h"
#include "Remote.h"

namespace osctruth
{
	// The `listeners` of the a3-osc.json at `path`. Empty, with `error` set,
	// when the file is missing or does not parse -- the caller says so.
	std::vector<Listener> readListeners (const std::string& path, std::string& error);

	// The file's `hosts`: name -> address. Empty, with `error` set, likewise.
	std::map<std::string, std::string> readHosts (const std::string& path, std::string& error);

	// The patterns StemDeck speaks by remote control (spec stemdeck-remote).
	// A missing one stays empty, with `error` naming it.
	remote::Words readRemoteWords (const std::string& path, std::string& error);

	// The meter ballistics of the truth's "meters" block; the defaults when
	// the file or the block is missing (`error` set only for the file).
	MeterParameters readMeterParameters (const std::string& path, std::string& error);

	// The truth StemDeck reads (spec truth-from-core, step 3): $A3_OSC_TRUTH,
	// else what Core last served (~/.cache/a3/a3-osc.json) if it reads as a
	// truth, else the package's file.
	std::string liveTruthPath();

	// Why `text` cannot be StemDeck's truth, or empty.
	std::string unusableTruth (const std::string& text);
}
