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

//==============================================================================
// A set of 1-4 stereo files (2026-10-29): file n of a numbered group on slot n,
// the other slots empty; any other file a set of its own on slot 1.
namespace
{
	bool naturalLess (const std::string& a, const std::string& b) { return a < b; }

	// The sets of `names` as "name: slot1 slot2 slot3 slot4", a file's name or
	// "-" for an empty slot, sorted -- the order of the groups is not the rule.
	std::vector<std::string> describe (const std::vector<std::string>& names)
	{
		std::vector<std::string> sets;
		for (const auto& set : groupStemFiles (names, naturalLess))
		{
			auto text = set.name + ":";
			for (const auto& slot : set.slots)
				text += " " + (slot ? names[*slot] : std::string ("-"));
			sets.push_back (text);
		}
		std::sort (sets.begin(), sets.end());
		return sets;
	}

	using Sets = std::vector<std::string>;
}

TEST (StemGrouping, FourCreatorFilesAreOneSetInSlotOrder)
{
	EXPECT_EQ (describe ({ "T - 3 - other", "T - 1 - drums", "T - 4 - vocals", "T - 2 - bass" }),
			   (Sets { "T: T - 1 - drums T - 2 - bass T - 3 - other T - 4 - vocals" }));
}

TEST (StemGrouping, ThreeOrTwoNumberedFilesFillTheirSlotsAndLeaveTheRestEmpty)
{
	EXPECT_EQ (describe ({ "T - 1 - a", "T - 2 - b", "T - 3 - c" }),
			   (Sets { "T: T - 1 - a T - 2 - b T - 3 - c -" }));
	EXPECT_EQ (describe ({ "T - 3 - other", "T - 1 - drums" }),
			   (Sets { "T: T - 1 - drums - T - 3 - other -" })) << "the number is the slot";
	EXPECT_EQ (describe ({ "T - 1.drums", "T - 2.bass" }), (Sets { "T: T - 1.drums T - 2.bass - -" }));
}

TEST (StemGrouping, ALoneStereoFileIsASetOfOneOnSlotOne)
{
	EXPECT_EQ (describe ({ "Track" }), (Sets { "Track: Track - - -" })) << "no separator to split at";
	EXPECT_EQ (describe ({ "Artist - Title" }), (Sets { "Artist - Title: Artist - Title - - -" }));
	EXPECT_EQ (describe ({ "T - 3 - other" }), (Sets { "T - 3 - other: T - 3 - other - - -" }))
		<< "one file alone is a stereo file, whatever its number";
}

TEST (StemGrouping, AlbumNumbersNeverMakeASet)
{
	EXPECT_EQ (describe ({ "01 - Intro", "02 - Song", "03 - Outro" }),
			   (Sets { "01 - Intro: 01 - Intro - - -", "02 - Song: 02 - Song - - -", "03 - Outro: 03 - Outro - - -" }));
	EXPECT_EQ (describe ({ "Artist - 01 - Intro", "Artist - 02 - Song" }),
			   (Sets { "Artist - 01 - Intro: Artist - 01 - Intro - - -", "Artist - 02 - Song: Artist - 02 - Song - - -" }))
		<< "a leading zero is a track number, not a slot";
	EXPECT_EQ (describe ({ "Album - 01", "Album - 02", "Album - 03" }),
			   (Sets { "Album - 01: Album - 01 - - -", "Album - 02: Album - 02 - - -", "Album - 03: Album - 03 - - -" }))
		<< "a bare number is no stem name";
}

TEST (StemGrouping, NumbersBeyondFourOrTwiceAreNoSet)
{
	EXPECT_EQ (describe ({ "T - 1 - a", "T - 5 - e" }), (Sets { "T - 1 - a: T - 1 - a - - -", "T - 5 - e: T - 5 - e - - -" }));
	EXPECT_EQ (describe ({ "T - 1 - a", "T - 1 - b" }), (Sets { "T - 1 - a: T - 1 - a - - -", "T - 1 - b: T - 1 - b - - -" }));
	EXPECT_EQ (describe ({ "Mix A", "Mix B" }).size(), 2u) << "two files sharing a word are two tracks";
}

TEST (StemGrouping, FourFilesOfOnePrefixStayASetAsBefore)
{
	EXPECT_EQ (describe ({ "S - PERC", "S - DUB", "S - PADS", "S - KICK" }), (Sets { "S: S - DUB S - KICK S - PADS S - PERC" }));
	EXPECT_EQ (describe ({ "S-004", "S-001", "S-003", "S-002" }), (Sets { "S: S-001 S-002 S-003 S-004" }));
}

TEST (StemGrouping, FiveFilesOfOnePrefixAreFiveSets)
{
	EXPECT_EQ (describe ({ "A - 01", "A - 02", "A - 03", "A - 04", "A - 05" }).size(), 5u);
}

TEST (StemGrouping, TheSlotsStemNamesAreTheSuffixesOrTheWholeName)
{
	const std::vector<std::string> names { "T - 2 - bass", "T - 4 - vocals", "Lone" };
	auto sets = groupStemFiles (names, naturalLess);
	std::sort (sets.begin(), sets.end(), [] (const auto& a, const auto& b) { return a.name < b.name; });
	ASSERT_EQ (sets.size(), 2u);
	EXPECT_EQ (sets[0].name, "Lone");
	EXPECT_EQ (sets[0].stemNames, (std::array<std::string, 4> { "Lone", "", "", "" }));
	EXPECT_EQ (sets[1].stemNames, (std::array<std::string, 4> { "", "2 - bass", "", "4 - vocals" }));
}

TEST (StemGrouping, ASlotNumberIsOneDigitOneToFourBeforeAName)
{
	EXPECT_EQ (numberedSlot ("1 - drums"), 0);
	EXPECT_EQ (numberedSlot ("4.vocals"), 3);
	EXPECT_EQ (numberedSlot ("2 - Big Bass"), 1);
	for (const auto* notOne : { "01 - drums", "5 - x", "0 - x", "2", "2 - ", "DUB", "12 - x", "" })
		EXPECT_FALSE (numberedSlot (notOne).has_value()) << notOne;
}

TEST (StemGrouping, TheLoadedSlotsAreShownByNumber)
{
	EXPECT_EQ (loadedSlotsText ({ true, true, true, true }), "1 2 3 4");
	EXPECT_EQ (loadedSlotsText ({ true, false, false, false }), "1");
	EXPECT_EQ (loadedSlotsText ({ false, true, false, true }), "2 4");
}
