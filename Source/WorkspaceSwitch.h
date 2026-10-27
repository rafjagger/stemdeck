// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "WorkspacePanel.h"
#include "Workspaces.h"

#include <juce_gui_basics/juce_gui_basics.h>

// The way out of StemDeck's workspace for a touch user: the MOTION key (over
// to A3 Motion, i3 workspace 1) and the arrow that lists all of the rig's
// workspaces. Shared by the two-deck and the one-deck screen, so both look
// and behave alike; sits where A3 Motion has it (switcherGeometry).
class WorkspaceSwitch
{
public:
	explicit WorkspaceSwitch (I3Runner = systemI3Runner());

	// Adds the keys, and the list laid over the whole of `parent`, to it.
	void attachTo (juce::Component& parent);

	// Puts the keys at the right end of the window, in the height of `topBar`, and returns the rest of
	// `topBar`, `gap` short of the MOTION key.
	juce::Rectangle<int> place (juce::Rectangle<int> topBar, int gap);

	juce::TextButton& motionButton() { return motion; }
	juce::TextButton& workspacesButton() { return arrow; }
	WorkspacePanel& panel() { return list; }

private:
	void showList();

	I3Runner i3;
	juce::Component* window = nullptr;
	juce::TextButton motion { "MOTION" };
	juce::TextButton arrow { juce::String::fromUTF8 ("\xe2\x96\xbe") };
	WorkspacePanel list;
};
