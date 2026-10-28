// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "PanelFollowsWorkspace.h"
#include "PanelOwnership.h"

#include <filesystem>
#include <unistd.h>

namespace
{
	using Action = PanelFollow::Action;
	const std::string mine = "2:STEMDECK";

	struct TempLock
	{
		std::filesystem::path path = std::filesystem::temp_directory_path() / ("stemdeck-follow-" + std::to_string (::getpid())) / "panel.lock";
		TempLock() { std::filesystem::remove_all (path.parent_path()); }
		~TempLock() { std::filesystem::remove_all (path.parent_path()); }
	};
}

TEST (PanelFollowsWorkspace, ReleasesAtOnceWhenAnotherOfOursIsFocused)
{
	PanelFollowsWorkspace rule (mine);
	EXPECT_EQ (rule.update ("1:MOTION", 10.0, true), Action::release);
}

TEST (PanelFollowsWorkspace, TakesOnlyAfterTheDelay)
{
	PanelFollowsWorkspace rule (mine);
	EXPECT_EQ (rule.update (mine, 10.0, false), Action::none);
	EXPECT_EQ (rule.update (mine, 10.5, false), Action::none);
	EXPECT_EQ (rule.update (mine, 10.75, false), Action::takeAfterDelay);
}

TEST (PanelFollowsWorkspace, ShortVisitTakesNothing)
{
	PanelFollowsWorkspace rule (mine);
	EXPECT_EQ (rule.update (mine, 10.0, false), Action::none);
	EXPECT_EQ (rule.update (mine, 10.5, false), Action::none);
	EXPECT_EQ (rule.update ("1:MOTION", 10.6, false), Action::none);
	// coming back starts the count again
	EXPECT_EQ (rule.update (mine, 11.0, false), Action::none);
	EXPECT_EQ (rule.update (mine, 11.5, false), Action::none);
}

TEST (PanelFollowsWorkspace, ForeignWorkspaceChangesNothing)
{
	PanelFollowsWorkspace rule (mine);
	EXPECT_EQ (rule.update ("3:ENGINE", 10.0, true), Action::none);
	EXPECT_EQ (rule.update ("3:ENGINE", 20.0, false), Action::none);
}

TEST (PanelFollowsWorkspace, HoldingAndFocusedChangesNothing)
{
	PanelFollowsWorkspace rule (mine);
	EXPECT_EQ (rule.update (mine, 10.0, true), Action::none);
	EXPECT_EQ (rule.update (mine, 20.0, true), Action::none);
}

TEST (PanelFollowsWorkspace, WithoutTheVariableNothingChanges)
{
	EXPECT_FALSE (panelWorkspaceFromEnvironment ("").has_value());
	EXPECT_EQ (panelWorkspaceFromEnvironment ("2:STEMDECK"), std::optional<std::string> ("2:STEMDECK"));
}

// The wiring through the real lock: the seam the component calls.
TEST (PanelFollowsWorkspace, ActionsTakeAndReleaseTheRealLock)
{
	TempLock lock;
	panel::Ownership ownership (lock.path, 'B');
	EXPECT_FALSE (panel::applyFollow (ownership, Action::none));
	EXPECT_FALSE (std::filesystem::exists (lock.path));   // nothing touched without a reason
	EXPECT_TRUE (panel::applyFollow (ownership, Action::takeAfterDelay));
	EXPECT_TRUE (ownership.holds());
	EXPECT_TRUE (panel::applyFollow (ownership, Action::none));
	EXPECT_FALSE (panel::applyFollow (ownership, Action::release));
	EXPECT_FALSE (ownership.holds());
}

// Motion may open the tty 0.75 s after its workspace came to the front; this
// side must have closed it by then: the poll plus one tick of the message thread.
TEST (PanelFollowsWorkspace, ReleaseIsFasterThanTheOtherSidesTakeDelay)
{
	EXPECT_LT (panelPollSeconds + panelReactSeconds, 0.75);
}

TEST (PanelFollowsWorkspace, GainingThePanelRearmsThePots)
{
	int rearmed = 0;
	panel::PanelGain gain ([&] { ++rearmed; });
	gain.update (false);
	EXPECT_EQ (rearmed, 0);
	gain.update (true);
	EXPECT_EQ (rearmed, 1);
	gain.update (true);
	EXPECT_EQ (rearmed, 1);   // still the same holding
	gain.update (false);
	gain.update (true);
	EXPECT_EQ (rearmed, 2);   // gained again
}

TEST (PanelFollowsWorkspace, TheToggleShowsWhyItIsInertInFollowMode)
{
	const auto follow = panel::toggleFace (true, true);
	EXPECT_FALSE (follow.enabled);
	EXPECT_TRUE (follow.on);
	EXPECT_NE (follow.text.find ("workspace"), std::string::npos);

	const auto plain = panel::toggleFace (false, false);
	EXPECT_TRUE (plain.enabled);
	EXPECT_FALSE (plain.on);
	EXPECT_EQ (plain.text, "Panel: this deck");
}
