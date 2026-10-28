// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Workspaces.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <string>

// Asks i3 every 500 ms which workspace is in front (plan R9). Its own thread,
// because asking i3 starts a process and must never run on the audio or message
// thread. A stuck i3-msg is waited for 2 s at shutdown at most.
class PanelWorkspacePoller : public juce::Thread
{
public:
	explicit PanelWorkspacePoller (I3Runner runner = systemI3Runner());
	~PanelWorkspacePoller() override { stopThread (2000); }

	// "2:STEMDECK", the way the variable and i3's config spell it; "" without an answer.
	std::string focused() const;
	// Counts up whenever the answer changed, so the message thread can look every tick without a lock.
	unsigned generation() const { return changes.load(); }

	void run() override;

private:
	I3Runner i3;
	mutable juce::CriticalSection lock;
	std::string focusedName;
	std::atomic<unsigned> changes { 0 };
};

// The focused workspace spelled like the variable; "" when none is.
std::string focusedWorkspaceName (const std::vector<WorkspaceEntry>&);
