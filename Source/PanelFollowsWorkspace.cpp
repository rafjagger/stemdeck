// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "PanelFollowsWorkspace.h"

#include <algorithm>

PanelFollowsWorkspace::PanelFollowsWorkspace (std::string myWorkspace, double takeDelaySeconds, std::vector<std::string> sharing)
	: mine (std::move (myWorkspace)), takeDelay (takeDelaySeconds), decks (std::move (sharing))
{
}

PanelFollow::Action PanelFollowsWorkspace::update (const std::string& focusedWorkspace, double nowSeconds, bool holding)
{
	using Action = PanelFollow::Action;

	if (focusedWorkspace == mine)
	{
		if (holding)
			return Action::none;
		if (! frontSince)
			frontSince = nowSeconds;
		// Longer than one poll of the other side, so its port is closed by the time ours opens.
		return nowSeconds - *frontSince >= takeDelay ? Action::takeAfterDelay : Action::none;
	}

	frontSince.reset();
	const auto isOtherDeck = std::find (decks.begin(), decks.end(), focusedWorkspace) != decks.end();
	return isOtherDeck && holding ? Action::release : Action::none;
}

std::optional<std::string> panelWorkspaceFromEnvironment (const std::string& value)
{
	if (value.empty())
		return std::nullopt;
	return value;
}
