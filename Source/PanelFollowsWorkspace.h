// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>
#include <string>
#include <vector>

// How fast a release follows a focus change: the i3 poll, then one tick of the
// message thread. Must stay below the other side's take delay (0.75 s).
inline constexpr double panelPollSeconds = 0.5;
inline constexpr double panelReactSeconds = 1.0 / 60.0;

struct PanelFollow
{
	enum class Action
	{
		none,
		release,          // let the panel go at once
		takeAfterDelay    // our workspace has been in front long enough: take it
	};
};

// Which program holds the shared hardware panel on a3nuc2 (plan R9): the one
// whose i3 workspace is in front. A³ Motion UI has the same rule, ported; no
// code is shared. Pure: the caller reads i3 and the clock and acts on the answer.
class PanelFollowsWorkspace
{
public:
	// `decks` are the workspaces of the programs that share the panel; a
	// workspace outside them never changes anything.
	explicit PanelFollowsWorkspace (std::string myWorkspace, double takeDelaySeconds = 0.75,
									std::vector<std::string> decks = { "1:MOTION", "2:STEMDECK" });

	PanelFollow::Action update (const std::string& focusedWorkspace, double nowSeconds, bool holding);

private:
	std::string mine;
	double takeDelay;
	std::vector<std::string> decks;
	std::optional<double> frontSince;   // while our workspace is in front
};

// STEMDECK_PANEL_WORKSPACE: nothing when unset or empty -- then the panel is held as it always was.
std::optional<std::string> panelWorkspaceFromEnvironment (const std::string& value);
