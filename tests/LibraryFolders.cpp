#include <gtest/gtest.h>

#include "LibraryFolders.h"
#include "TapFilter.h"

namespace
{
	// Each set's folder relative to the library, as the library has them;
	// "" for a set straight in the library folder.
	const std::vector<std::string> sets { "tidal/ra_mok", "tidal/ra_mok", "tidal/ra_niceone", "Joktbach/one", "", "tidal" };
}

TEST (LibraryFolders, TheRootShowsEverySet)
{
	for (const auto& folder : sets)
		EXPECT_TRUE (libraryfolders::isWithin (folder, ""));
	EXPECT_EQ (libraryfolders::countWithin (sets, ""), sets.size());
}

TEST (LibraryFolders, AFolderShowsItsSetsAndThoseOfItsSubfolders)
{
	EXPECT_TRUE (libraryfolders::isWithin ("tidal/ra_mok", "tidal"));
	EXPECT_TRUE (libraryfolders::isWithin ("tidal", "tidal"));
	EXPECT_TRUE (libraryfolders::isWithin ("tidal/ra_mok", "tidal/ra_mok"));
	EXPECT_FALSE (libraryfolders::isWithin ("tidal/ra_niceone", "tidal/ra_mok"));
	EXPECT_FALSE (libraryfolders::isWithin ("Joktbach/one", "tidal"));
	EXPECT_FALSE (libraryfolders::isWithin ("", "tidal")) << "a set in the root is in no subfolder";
	EXPECT_EQ (libraryfolders::countWithin (sets, "tidal"), 4u);
}

TEST (LibraryFolders, ANameThatOnlyStartsTheSameIsAnotherFolder)
{
	EXPECT_FALSE (libraryfolders::isWithin ("tidal2/x", "tidal"));
	EXPECT_FALSE (libraryfolders::isWithin ("tidalwave", "tidal"));
}

TEST (LibraryFolders, TheTreeHoldsEveryFolderWithASetAndItsParentsParentsFirst)
{
	const std::vector<std::string> deep { "a/b/c", "x" };
	EXPECT_EQ (libraryfolders::folderTree (deep), (std::vector<std::string> { "a", "a/b", "a/b/c", "x" }));

	EXPECT_EQ (libraryfolders::folderTree (sets),
			   (std::vector<std::string> { "Joktbach", "Joktbach/one", "tidal", "tidal/ra_mok", "tidal/ra_niceone" }))
		<< "no duplicates, no root entry, case-insensitive order";
}

TEST (LibraryFolders, SiblingsSortByNameNotByPathText)
{
	// "a/b" before "a-z": a plain string sort puts '-' (0x2d) before '/' (0x2f).
	EXPECT_EQ (libraryfolders::folderTree ({ "a-z", "a/b" }), (std::vector<std::string> { "a", "a/b", "a-z" }));
}

TEST (LibraryFolders, AChoiceThatIsGoneFallsBackToTheRoot)
{
	const auto tree = libraryfolders::folderTree (sets);
	EXPECT_EQ (libraryfolders::validChoice ("tidal/ra_mok", tree), "tidal/ra_mok");
	EXPECT_EQ (libraryfolders::validChoice ("tidal/gone", tree), "");
	EXPECT_EQ (libraryfolders::validChoice ("", tree), "");
}

TEST (LibraryFolders, TheLastPartIsTheName)
{
	EXPECT_EQ (libraryfolders::nameOf ("tidal/ra_mok"), "ra_mok");
	EXPECT_EQ (libraryfolders::nameOf ("tidal"), "tidal");
	EXPECT_EQ (libraryfolders::parentOf ("tidal/ra_mok"), "tidal");
	EXPECT_EQ (libraryfolders::parentOf ("tidal"), "");
}

//==============================================================================
// On the rig every finger arrives twice, as a touch and as X's emulated mouse,
// a few ms apart: a toggle must not see both.
TEST (TapFilter, TheTwinOfATapIsDropped)
{
	TapFilter filter;
	EXPECT_TRUE (filter.accept (1000.0));
	EXPECT_FALSE (filter.accept (1002.0));
	EXPECT_FALSE (filter.accept (1000.0 + TapFilter::twinWindowMs - 1.0));
}

TEST (TapFilter, ASecondTapByHandCounts)
{
	TapFilter filter;
	EXPECT_TRUE (filter.accept (1000.0));
	EXPECT_TRUE (filter.accept (1000.0 + TapFilter::twinWindowMs + 1.0));
	EXPECT_TRUE (filter.accept (5000.0));
}

TEST (SortTap, ANewColumnSortsForwardsTheSameOneReverses)
{
	EXPECT_EQ (sortAfterTap ({ 5, true }, 3), (SortOrder { 3, true }));
	EXPECT_EQ (sortAfterTap ({ 3, true }, 3), (SortOrder { 3, false }));
	EXPECT_EQ (sortAfterTap ({ 3, false }, 3), (SortOrder { 3, true }));
	EXPECT_EQ (sortAfterTap ({ 3, false }, 0), (SortOrder { 3, false })) << "a tap beside every column changes nothing";
}

// A finger on a touch list: a quick swipe scrolls, a press held still first
// drags the row (to a deck); a press that ends near where it began is a tap.
TEST (TouchList, AHeldPressDragsAQuickOneScrolls)
{
	EXPECT_FALSE (touchlist::isLongPress (40.0));
	EXPECT_FALSE (touchlist::isLongPress (touchlist::longPressMs - 1.0));
	EXPECT_TRUE (touchlist::isLongPress (touchlist::longPressMs));
	EXPECT_TRUE (touchlist::isLongPress (900.0));
}

TEST (TouchList, ATapEndsWithinAQuarterRow)
{
	EXPECT_TRUE (touchlist::isTap (0.0f, 24));
	EXPECT_TRUE (touchlist::isTap (5.0f, 24));
	EXPECT_FALSE (touchlist::isTap (6.0f, 24));
	EXPECT_FALSE (touchlist::isTap (40.0f, 24)) << "that was a scroll";
}
