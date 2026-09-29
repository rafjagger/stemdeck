#include <gtest/gtest.h>

#include "StemNames.h"

#include <algorithm>
#include <array>

TEST (StemNames, TheCreatorsNamesAreOneSetInBusOrder)
{
	std::array<std::string, 4> suffixes;
	for (int stem = 0; stem < 4; ++stem)
	{
		const auto file = stemFileName ("Burial - Archangel", stem, "flac");
		const auto base = file.substr (0, file.rfind ('.'));
		std::string prefix, suffix;
		ASSERT_TRUE (splitStemName (base, prefix, suffix)) << file;
		EXPECT_EQ (prefix, "Burial - Archangel") << file;
		suffixes[(size_t) stem] = suffix;
	}
	EXPECT_EQ (stemFileName ("Archangel", 0, "wav"), "Archangel - 1 - drums.wav");
	EXPECT_EQ (stemFileName ("Archangel", 3, "flac"), "Archangel - 4 - vocals.flac");
	auto sorted = suffixes;
	std::sort (sorted.begin(), sorted.end());
	EXPECT_EQ (sorted, suffixes) << "sorted, they stay drums, bass, other, vocals";
}

TEST (StemNames, ASpaceBeforeTheStemNameWouldBreakTheSet)
{
	std::string p1, s1, p2, s2;
	ASSERT_TRUE (splitStemName ("Title - 1 drums", p1, s1));
	ASSERT_TRUE (splitStemName ("Title - 2 bass", p2, s2));
	EXPECT_NE (p1, p2) << "why the creator writes 1 - drums, not 1 drums";
}

TEST (StemNames, TheOldCreatorNamesStillMakeASet)
{
	std::string prefix, suffix;
	ASSERT_TRUE (splitStemName ("Archangel - 1.drums", prefix, suffix));
	EXPECT_EQ (prefix, "Archangel");
	EXPECT_EQ (suffix, "1.drums");
}

TEST (StemNames, NamedStemsWithoutANumberSplitAsBefore)
{
	std::string prefix, suffix;
	ASSERT_TRUE (splitStemName ("Lukas - TRIPLE A - DUB", prefix, suffix));
	EXPECT_EQ (prefix, "Lukas - TRIPLE A");
	EXPECT_EQ (suffix, "DUB");
	ASSERT_TRUE (splitStemName ("Set - 01 - 02", prefix, suffix));
	EXPECT_EQ (prefix, "Set - 01") << "two numbers: the last one is the stem";
	EXPECT_EQ (suffix, "02");
}

TEST (StemNames, SplitIsAtTheLastSeparator)
{
	std::string prefix, suffix;
	ASSERT_TRUE (splitStemName ("Artist - Title-001", prefix, suffix));
	EXPECT_EQ (prefix, "Artist - Title");
	EXPECT_EQ (suffix, "001");
	EXPECT_FALSE (splitStemName ("NoSeparator", prefix, suffix));
	EXPECT_FALSE (splitStemName ("Trailing-", prefix, suffix));
}

TEST (StemNames, NamesAreSanitised)
{
	EXPECT_EQ (sanitiseName ("AC/DC", "x"), "AC_DC");
	EXPECT_EQ (sanitiseName ("..hidden", "x"), "hidden");
	EXPECT_EQ (sanitiseName ("Title - ", "x"), "Title");
	EXPECT_EQ (sanitiseName ("  Mix_ ", "x"), "Mix");
	EXPECT_EQ (sanitiseName ("", "Unknown Artist"), "Unknown Artist");
	EXPECT_EQ (sanitiseName ("///", "Unknown Album"), "Unknown Album");
	EXPECT_EQ (sanitiseName ("Sigur R\xc3\xb3s", "x"), "Sigur R\xc3\xb3s") << "unicode kept";
	EXPECT_EQ (sanitiseName (std::string ("a\0b", 3), "x"), "a_b");
}
