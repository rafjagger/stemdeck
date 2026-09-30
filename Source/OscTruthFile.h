#pragma once

#include "OscTruth.h"

namespace osctruth
{
	// The `listeners` of the a3-osc.json at `path`. Empty, with `error` set,
	// when the file is missing or does not parse -- the caller says so.
	std::vector<Listener> readListeners (const std::string& path, std::string& error);
}
