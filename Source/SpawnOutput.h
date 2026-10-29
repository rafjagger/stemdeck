// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

// Runs argv[0] (looked up in PATH) and returns what it printed to stdout;
// "" when it cannot start or has not finished within `timeoutMs` (it is
// killed then). Never fork(): the deck's decoded stems make its address
// space hundreds of MB, and copying that stalls the audio thread.
std::string spawnForOutput (const std::vector<std::string>& argv, int timeoutMs);
