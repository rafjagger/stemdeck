#include <gtest/gtest.h>

#include "DataPaths.h"

TEST (DataPaths, RecordingsLiveInTheUsersDataFolder)
{
	EXPECT_EQ (recordingsFolder ("/home/aaa", nullptr), "/home/aaa/.local/share/stemdeck/recordings");
	EXPECT_EQ (dataFolder ("/home/aaa", nullptr), "/home/aaa/.local/share/stemdeck");
}

TEST (DataPaths, AnAbsoluteXdgDataHomeWins)
{
	EXPECT_EQ (recordingsFolder ("/home/aaa", "/data/xdg"), "/data/xdg/stemdeck/recordings");
}

TEST (DataPaths, AnEmptyOrRelativeXdgDataHomeIsIgnored)
{
	// The XDG spec: a relative $XDG_DATA_HOME is invalid and must be ignored.
	EXPECT_EQ (recordingsFolder ("/home/aaa", ""), "/home/aaa/.local/share/stemdeck/recordings");
	EXPECT_EQ (recordingsFolder ("/home/aaa", "data"), "/home/aaa/.local/share/stemdeck/recordings");
}

TEST (DataPaths, TrailingSlashesMakeNoDoubleSlash)
{
	EXPECT_EQ (recordingsFolder ("/home/aaa/", nullptr), "/home/aaa/.local/share/stemdeck/recordings");
	EXPECT_EQ (recordingsFolder ("/home/aaa", "/data/xdg/"), "/data/xdg/stemdeck/recordings");
}
