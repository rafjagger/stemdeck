// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "PanelOwnership.h"

#include <filesystem>
#include <fstream>
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
