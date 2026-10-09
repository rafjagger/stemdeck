#include <gtest/gtest.h>

#include "CachedGrid.h"

using CachedGrid::Entry;

namespace
{
	Entry analysedBefore() { return { 126.0, 0.31, 1, false }; }
}

TEST (CachedGrid, AGridFromBeforeGetsItsOneFoundAgain)
{
	EXPECT_TRUE (CachedGrid::needsNewDownbeat (analysedBefore()));
}

TEST (CachedGrid, AGridCorrectedByHandIsNeverFoundAgain)
{
	auto entry = analysedBefore();
	entry.corrected = true;
	EXPECT_FALSE (CachedGrid::needsNewDownbeat (entry));
}

TEST (CachedGrid, ACurrentGridIsLeftAlone)
{
	auto entry = analysedBefore();
	entry.version = CachedGrid::currentVersion;
	EXPECT_FALSE (CachedGrid::needsNewDownbeat (entry));
}

TEST (CachedGrid, NoTempoNothingToFind)
{
	auto entry = analysedBefore();
	entry.bpm = 0.0;
	EXPECT_FALSE (CachedGrid::needsNewDownbeat (entry));
}

TEST (CachedGrid, TheNewOneKeepsTheTempoAndIsCurrent)
{
	const auto entry = CachedGrid::withNewDownbeat (analysedBefore(), 1.26);
	EXPECT_DOUBLE_EQ (entry.bpm, 126.0);
	EXPECT_DOUBLE_EQ (entry.firstBeat, 1.26);
	EXPECT_EQ (entry.version, CachedGrid::currentVersion);
	EXPECT_FALSE (CachedGrid::needsNewDownbeat (entry)) << "found once, not on every load";
}

TEST (CachedGrid, ACorrectionMadeMeanwhileWins)
{
	// The DJ corrected the grid while its one was being found.
	auto entry = analysedBefore();
	entry.corrected = true;
	const auto after = CachedGrid::withNewDownbeat (entry, 1.26);
	EXPECT_DOUBLE_EQ (after.firstBeat, 0.31);
	EXPECT_EQ (after.version, 1) << "found again should the correction be reset";
}
