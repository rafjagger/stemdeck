// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "PanelOwnership.h"

#include <filesystem>
#include <fstream>
#include <climits>
#include <unistd.h>

namespace
{
	struct TempDir
	{
		std::filesystem::path path = std::filesystem::temp_directory_path() / ("stemdeck-panel-lock-" + std::to_string (::getpid()) + "-" + std::to_string (counter++));
		TempDir() { std::filesystem::remove_all (path); }
		~TempDir() { std::filesystem::remove_all (path); }
		std::filesystem::path lock() const { return path / "StemDeck" / "panel.lock"; }
		static inline int counter = 0;
	};
}

TEST (PanelOwnership, OneInstanceHoldsIt)
{
	TempDir dir;
	panel::Ownership a (dir.lock(), 'A'), b (dir.lock(), 'B');

	EXPECT_TRUE (a.claimByDefault());   // no lock: deck A takes it
	EXPECT_TRUE (a.holds());
	EXPECT_FALSE (b.claimByDefault());  // B never takes it by default
	EXPECT_FALSE (b.holds());

	b.take();                           // the toggle on B
	EXPECT_TRUE (b.holds());
	EXPECT_FALSE (a.holds());           // A sees it go on its next look

	a.release();                        // A lets go of what it no longer holds: nothing happens
	EXPECT_TRUE (b.holds());

	b.release();                        // toggle off, or quit
	EXPECT_FALSE (b.holds());
	EXPECT_FALSE (std::filesystem::exists (dir.lock()));
}

TEST (PanelOwnership, DeckAClaimsOnlyWhenNoLockExists)
{
	TempDir dir;
	panel::Ownership a (dir.lock(), 'A'), b (dir.lock(), 'B');
	b.take();
	EXPECT_FALSE (a.claimByDefault());
	EXPECT_TRUE (b.holds());
}

TEST (PanelOwnership, ReleasedByQuitLeavesTheNextStartToDeckA)
{
	TempDir dir;
	{
		panel::Ownership a (dir.lock(), 'A');
		ASSERT_TRUE (a.claimByDefault());
		a.release();
	}
	panel::Ownership again (dir.lock(), 'A');
	EXPECT_TRUE (again.claimByDefault());
}

TEST (PanelOwnership, AnEmptyLockFileIsNoLock)
{
	TempDir dir;
	std::filesystem::create_directories (dir.lock().parent_path());
	{ std::ofstream (dir.lock()); }
	panel::Ownership a (dir.lock(), 'A');
	EXPECT_FALSE (a.holds());
	EXPECT_TRUE (a.claimByDefault());
}

TEST (PanelOwnership, ATakeMakesTheFolder)
{
	TempDir dir;
	panel::Ownership b (dir.lock(), 'B');
	b.take();
	EXPECT_TRUE (b.holds());
}

TEST (PanelOwnership, AnInstanceThatWantedItTakesItOnlyWhenFree)
{
	TempDir dir;
	panel::Ownership a (dir.lock(), 'A'), b (dir.lock(), 'B');
	ASSERT_TRUE (a.claimByDefault());
	EXPECT_FALSE (b.claimIfFree());
	EXPECT_TRUE (a.holds());
	a.release();
	EXPECT_TRUE (b.claimIfFree());
}

TEST (PanelOwnership, AValidLockOfAnotherIsNeverOverwrittenByAClaim)
{
	TempDir dir;
	panel::Ownership a (dir.lock(), 'A'), b (dir.lock(), 'B');
	b.take();
	EXPECT_FALSE (a.claimIfFree());
	EXPECT_FALSE (a.claimByDefault());
	EXPECT_TRUE (b.holds());
}

TEST (PanelOwnership, TwoClaimersOnAFreeLockLeaveOneHolder)
{
	TempDir dir;
	panel::Ownership a (dir.lock(), 'A'), b (dir.lock(), 'B');
	const auto first = b.claimIfFree();
	const auto second = a.claimIfFree();
	EXPECT_TRUE (first);
	EXPECT_FALSE (second);
	EXPECT_EQ ((int) a.holds() + (int) b.holds(), 1);
}

TEST (PanelOwnership, ALockOfADeadHolderIsFree)
{
	TempDir dir;
	panel::Ownership crashed (dir.lock(), 'B', INT_MAX);
	crashed.take();
	panel::Ownership a (dir.lock(), 'A');
	EXPECT_FALSE (a.holds());
	EXPECT_TRUE (a.claimByDefault());
	EXPECT_TRUE (a.holds());
	EXPECT_FALSE (crashed.holds());
}

TEST (PanelOwnership, ReleaseLeavesALockAnotherProcessTookUnderTheSameLetter)
{
	TempDir dir;
	panel::Ownership old (dir.lock(), 'B', ::getpid()), newer (dir.lock(), 'B', ::getppid());
	newer.take();
	old.release();
	EXPECT_TRUE (std::filesystem::exists (dir.lock()));
	EXPECT_TRUE (newer.holds());
}

TEST (PanelOwnership, TheStartDecisionIsPerDeck)
{
	for (const char letter : { 'A', 'B' })
	{
		const auto claims = [letter] (std::optional<bool> stored)
		{
			TempDir dir;
			panel::Ownership o (dir.lock(), letter);
			return panel::claimAtStart (o, stored);
		};
		EXPECT_EQ (claims (std::nullopt), letter == 'A') << letter;   // absent: A claims, B does not
		EXPECT_TRUE (claims (true)) << letter;                        // a stored value wins for that deck
		EXPECT_FALSE (claims (false)) << letter;
	}
}

TEST (PanelOwnership, AStoredOnClaimsOnlyWhenFree)
{
	TempDir dir;
	panel::Ownership a (dir.lock(), 'A'), b (dir.lock(), 'B');
	ASSERT_TRUE (panel::claimAtStart (a, std::nullopt));
	EXPECT_FALSE (panel::claimAtStart (b, true));
}

TEST (PanelLockPath, LivesUnderTheRuntimeDirWhenThereIsOne)
{
	TempDir dir;
	std::filesystem::create_directories (dir.path);
	EXPECT_EQ (panel::lockFilePath (dir.path.c_str(), 1000, dir.path / "tmp"), dir.path / "stemdeck-panel.lock");
}

TEST (PanelLockPath, FallsBackToAPrivateFolderPerUser)
{
	TempDir dir;
	std::filesystem::create_directories (dir.path);
	for (const char* none : { static_cast<const char*> (nullptr), "" })
	{
		const auto path = panel::lockFilePath (none, 1234, dir.path);
		EXPECT_EQ (path, dir.path / "stemdeck-1234" / "panel.lock");
		const auto mode = std::filesystem::status (path.parent_path()).permissions() & std::filesystem::perms::mask;
		EXPECT_EQ (mode, std::filesystem::perms::owner_all);
	}
}

TEST (PanelOwnership, ALockNamingALiveProcessOfAnotherProgramIsFree)
{
	TempDir dir;
	// pid 1 is alive and never stemdeck's own program.
	panel::Ownership stale (dir.lock(), 'B', 1);
	stale.take();
	panel::Ownership a (dir.lock(), 'A');
	EXPECT_TRUE (a.claimByDefault());
	EXPECT_TRUE (a.holds());
}

TEST (PanelOwnership, ALockNamingALiveProcessOfTheSameProgramIsHeld)
{
	TempDir dir;
	panel::Ownership other (dir.lock(), 'B');   // this very process stands in for another instance
	other.take();
	panel::Ownership a (dir.lock(), 'A');
	EXPECT_FALSE (a.claimByDefault());
}
