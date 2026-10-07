#include <gtest/gtest.h>

#include "FolderStep.h"

namespace
{
	const std::vector<std::string> albums { "/s/A/One", "/s/A/One", "/s/A/One", "/s/B/Two", "/s/B/Two" };
}

TEST (FolderStep, NextIsTheSetBelowInTheSameFolder)
{
	EXPECT_EQ (folderstep::neighbour (albums, 0, 1), std::optional<size_t> (1));
	EXPECT_EQ (folderstep::neighbour (albums, 3, 1), std::optional<size_t> (4));
}

TEST (FolderStep, PreviousIsTheSetAbove)
{
	EXPECT_EQ (folderstep::neighbour (albums, 2, -1), std::optional<size_t> (1));
}

TEST (FolderStep, TheFolderEndsAreEndsNoWrap)
{
	EXPECT_FALSE (folderstep::neighbour (albums, 2, 1)) << "the next folder is not the next track";
	EXPECT_FALSE (folderstep::neighbour (albums, 3, -1));
	EXPECT_FALSE (folderstep::neighbour (albums, 4, 1)) << "the list's end";
	EXPECT_FALSE (folderstep::neighbour (albums, 0, -1)) << "the list's start";
}

// Sorted by BPM, a folder's sets lie between others: those are skipped.
TEST (FolderStep, SetsOfOtherFoldersBetweenAreSkipped)
{
	const std::vector<std::string> byBpm { "/s/A", "/s/B", "/s/B", "/s/A", "/s/C" };
	EXPECT_EQ (folderstep::neighbour (byBpm, 0, 1), std::optional<size_t> (3));
	EXPECT_EQ (folderstep::neighbour (byBpm, 3, -1), std::optional<size_t> (0));
	EXPECT_FALSE (folderstep::neighbour (byBpm, 3, 1));
}

TEST (FolderStep, AnUnknownCurrentHasNoNeighbour)
{
	EXPECT_FALSE (folderstep::neighbour (albums, 9, 1));
	EXPECT_FALSE (folderstep::neighbour ({}, 0, 1));
}
