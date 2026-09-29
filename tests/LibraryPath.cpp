#include <gtest/gtest.h>

#include "LibraryPath.h"

TEST (LibraryPath, ArtistThenAlbum)
{
	const auto place = libraryPlaceOf ("Burial/Untrue");
	EXPECT_EQ (place.artist, "Burial");
	EXPECT_EQ (place.album, "Untrue");
}

TEST (LibraryPath, DeeperFoldersJoinTheAlbum)
{
	EXPECT_EQ (libraryPlaceOf ("Aphex Twin/Drukqs/CD1").album, "Drukqs / CD1");
}

TEST (LibraryPath, OneLevelDownIsOnlyAnArtist)
{
	const auto place = libraryPlaceOf ("Four Tet");
	EXPECT_EQ (place.artist, "Four Tet");
	EXPECT_EQ (place.album, "");
}

TEST (LibraryPath, StraightInTheLibraryIsNeither)
{
	const auto place = libraryPlaceOf ("");
	EXPECT_EQ (place.artist, "");
	EXPECT_EQ (place.album, "");
}

TEST (LibraryPath, StraySlashesDoNotMakeEmptyLevels)
{
	const auto place = libraryPlaceOf ("/Burial//Untrue/");
	EXPECT_EQ (place.artist, "Burial");
	EXPECT_EQ (place.album, "Untrue");
}
