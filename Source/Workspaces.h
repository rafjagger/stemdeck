#pragma once

#include <string>

// The rig's i3 workspaces as StemDeck's switch shows them. Their names are
// set in a3-core's i3 config ("1:MOTION", "2:STEMDECK", ...); the switch
// reads them from i3 rather than keeping a list of its own.

// "3:REAPER" -> "REAPER"; a name without a label keeps its number ("7").
std::string workspaceLabel (const std::string& name);

// The number i3 goes to for a name ("3:REAPER" -> 3); 0 without one.
int workspaceNumber (const std::string& name);

// Where the workspace switch sits: at the far right of the window, the key
// for the other app, then the arrow for the list. The same numbers stand in
// A3 Motion's io/Workspaces.hh -- both apps fill the same screen, so the key
// under the finger stays put when the workspace changes. Shares of the
// window's width; at the rig's 768 px they are StemDeck's first sizes.
struct SwitcherGeometry
{
	int margin = 0;        // from the window's right and top edges
	int arrowWidth = 0;
	int gap = 0;           // between the app key and the arrow
	int appKeyWidth = 0;
	int listTop = 0;       // the list's top, from the window's top
	int listWidth = 0;
	int listKeyHeight = 0;
	int listGap = 0;       // around and between the list's keys
};

SwitcherGeometry switcherGeometry (int windowWidth);
