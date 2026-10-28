// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "TransportDeck.h"

// CUE and PLAY together, like a Pioneer CDJ-3000's (manual, "Cueing"). Pure:
// no JUCE; the screen keys, the panels and the keyboard all come through here.
//
//   CUE down, playing:                back to the cue point, stopped
//   CUE down, stopped off the cue:    the cue point here
//   CUE down, stopped on the cue:     plays while held (the preview)
//   CUE up after a preview:           back to the cue point, stopped
//   PLAY while CUE is held:           plays on from where it is; the release
//                                     then leaves it playing
//   PLAY otherwise:                   play / pause
//
// A down while held or an up while not held is no new press: every touch
// arrives twice (touch#0 and X's emulated mouse#0), and the panel and the
// screen may hold CUE at once.
class CuePlay
{
public:
	void cueDown (TransportDeck&);
	void cueUp (TransportDeck&);
	void playPressed (TransportDeck&);

	bool isCueHeld() const { return held; }
	// A new track: whatever was being previewed is over.
	void reset() { previewing = false; }

private:
	bool held = false, previewing = false;
};
