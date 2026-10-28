// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "PanelWorkspacePoller.h"

std::string focusedWorkspaceName (const std::vector<WorkspaceEntry>& all)
{
	for (const auto& workspace : all)
		if (workspace.current)
			return std::to_string (workspace.number) + ":" + workspace.label;
	return {};
}

PanelWorkspacePoller::PanelWorkspacePoller (I3Runner runner)
	: juce::Thread ("PanelWorkspacePoller"), i3 (std::move (runner))
{
	startThread();
}

std::string PanelWorkspacePoller::focused() const
{
	const juce::ScopedLock guard (lock);
	return focusedName;
}

void PanelWorkspacePoller::run()
{
	while (! threadShouldExit())
	{
		const auto name = focusedWorkspaceName (listWorkspaces (i3));
		{
			const juce::ScopedLock guard (lock);
			focusedName = name;
		}
		wait (500);
	}
}
