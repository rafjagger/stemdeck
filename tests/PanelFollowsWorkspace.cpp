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
TEST (PanelFollowsWorkspace, StartingInFollowModeNeverTouchesTheLock)
{
	TempLock lock;
	panel::Ownership ownership (lock.path, 'B');
	EXPECT_FALSE (panel::startFollowing (ownership));
	EXPECT_FALSE (std::filesystem::exists (lock.path));
}

TEST (PanelFollowsWorkspace, ActionsTakeAndReleaseTheRealLock)
{
	TempLock lock;
	panel::Ownership ownership (lock.path, 'B');
	EXPECT_FALSE (panel::applyFollow (ownership, Action::none));
	EXPECT_TRUE (panel::applyFollow (ownership, Action::takeAfterDelay));
	EXPECT_TRUE (ownership.holds());
	EXPECT_TRUE (panel::applyFollow (ownership, Action::none));
	EXPECT_FALSE (panel::applyFollow (ownership, Action::release));
	EXPECT_FALSE (ownership.holds());
}
