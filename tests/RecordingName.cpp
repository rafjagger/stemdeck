#include <gtest/gtest.h>

#include "RecordingName.h"

TEST (RecordingName, SortsByTimeAndIsSafe)
{
	EXPECT_EQ (recordingFileName (2026, 9, 29, 19, 5, 12), "StemDeck 2026-09-29 19-05-12.flac");
	EXPECT_LT (recordingFileName (2026, 9, 29, 9, 0, 0), recordingFileName (2026, 9, 29, 10, 0, 0)) << "zero-padded: sorts as time";
	EXPECT_EQ (recordingFileName (2026, 1, 2, 3, 4, 5).find_first_of (":/\\"), std::string::npos);
}
