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
}
