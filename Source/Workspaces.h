#pragma once

#include <string>

// The rig's i3 workspaces as StemDeck's switch shows them. Their names are
// set in a3-core's i3 config ("1:MOTION", "2:STEMDECK", ...); the switch
// reads them from i3 rather than keeping a list of its own.

// "3:REAPER" -> "REAPER"; a name without a label keeps its number ("7").
std::string workspaceLabel (const std::string& name);

// The number i3 goes to for a name ("3:REAPER" -> 3); 0 without one.
int workspaceNumber (const std::string& name);
