#pragma once

#include <string>

// Where StemDeck keeps what it makes while running: $XDG_DATA_HOME/stemdeck,
// or ~/.local/share/stemdeck when that is unset or not absolute. Not the
// working directory: a packaged StemDeck and a dev build from a checkout
// record to the same place. Pure: home and $XDG_DATA_HOME are passed in.
std::string dataFolder (const std::string& home, const char* xdgDataHome);

// dataFolder()/recordings: where Recorder writes its FLAC files.
std::string recordingsFolder (const std::string& home, const char* xdgDataHome);
