#include <gtest/gtest.h>

#include "Sections.h"
#include "SyntheticStems.h"

using sections::Section;

// One test per section on synthetic stems: the script is one letter a bar
// (SyntheticStems.h), the answer one letter a bar (G groove, U build,
// D drop, B breakdown).
namespace
{
	std::string sectionsOf (const std::string& bars)
	{
		const auto features = synthetic::featuresOf (synthetic::fromScript (bars));
		std::string letters;
		for (const auto& bar : sections::classify (sections::barLevels (features, synthetic::bpm, synthetic::firstBeat, {})))
			letters += bar.section == Section::groove ? 'G'
					 : bar.section == Section::build  ? 'U'
					 : bar.section == Section::drop   ? 'D'
													  : 'B';
		return letters;
	}
}

TEST (Sections, AFullSetIsGroove)
{
	EXPECT_EQ (sectionsOf ("FFFFFFFF"), "GGGGGGGG");
}

TEST (Sections, DrumsGoneIsABreakdown)
{
	EXPECT_EQ (sectionsOf ("FFFFQQQQQQQQFFFF").substr (4, 8), "BBBBBBBB");
}

TEST (Sections, RisingDrumsBeforeTheFullSetAreABuild)
{
	EXPECT_EQ (sectionsOf ("FFFFQQQQqqqqssssFFFF").substr (8, 8), "UUUUUUUU");
}

TEST (Sections, KickAndBassBackAfterABuildIsADrop)
{
	EXPECT_EQ (sectionsOf ("FFFFQQQQqqqqssssFFFF").substr (16), "DDDD");
}

TEST (Sections, AWholeTrack)
{
	EXPECT_EQ (sectionsOf ("FFFFFFFFFFFFFFFF" "QQQQQQQQ" "qqqqssss" "FFFFFFFFFFFFFFFF" "FFFFFFFF"),
			   "GGGGGGGGGGGGGGGG" "BBBBBBBB" "UUUUUUUU" "DDDDDDDDDDDDDDDD" "GGGGGGGG")
		<< "a drop lasts sixteen bars, then it is the groove again";
}

TEST (Sections, ADropNeedsNoBuildAfterABreakdown)
{
	EXPECT_EQ (sectionsOf ("FFFFFFFFQQQQFFFF"), "GGGGGGGGBBBBDDDD");
}

TEST (Sections, AOneBarGapIsAFillNotABreakdown)
{
	EXPECT_EQ (sectionsOf ("FFFFFFFQFFFFFFFF"), "GGGGGGGGGGGGGGGG");
}

TEST (Sections, TheBassComingInIsNotADrop)
{
	EXPECT_EQ (sectionsOf ("kkkkkkkkFFFFFFFF"), "GGGGGGGGGGGGGGGG");
}

TEST (Sections, ASteadySnareIsNotABuild)
{
	EXPECT_EQ (sectionsOf ("FFFFQQQQqqqqqqqqFFFF"), "GGGGBBBBGGGGGGGGDDDD")
		<< "no rise: groove; the breakdown before still makes the drop";
}

TEST (Sections, WithoutDrumsEverythingIsGroove)
{
	EXPECT_EQ (sectionsOf ("QQQQQQQQ"), "GGGGGGGG");
}

TEST (Sections, EnergyFollowsTheLevel)
{
	const auto features = synthetic::featuresOf (synthetic::fromScript ("FFFFQQQQ"));
	const auto bars = sections::classify (sections::barLevels (features, synthetic::bpm, synthetic::firstBeat, {}));
	ASSERT_EQ (bars.size(), 8u);
	EXPECT_GT (bars[0].energy, 0.9f);
	EXPECT_LT (bars[5].energy, 0.3f);
	for (const auto& bar : bars)
	{
		EXPECT_GE (bar.energy, 0.0f);
		EXPECT_LE (bar.energy, 1.0f);
	}
}

TEST (Sections, NothingInNothingOut)
{
	EXPECT_TRUE (sections::classify ({}).empty());
}

TEST (Sections, TheWordsOnTheWire)
{
	EXPECT_STREQ (sections::nameOf (Section::groove), "groove");
	EXPECT_STREQ (sections::nameOf (Section::build), "build");
	EXPECT_STREQ (sections::nameOf (Section::drop), "drop");
	EXPECT_STREQ (sections::nameOf (Section::breakdown), "breakdown");
}

// A set short of four stems (2026-10-29) holds anything: it is read as one
// mix, the sum of its slots on slot 1, which stands for both drums and bass.
TEST (Sections, ASetShortOfFourStemsIsReadAsOneMix)
{
	auto stems = synthetic::fromScript ("FFFFFFFFQQQQFFFF");
	sections::foldIntoOneMix (stems.samples);
	for (size_t s = 1; s < stems.samples.size(); ++s)
		for (const auto sample : stems.samples[s])
			ASSERT_EQ (sample, 0.0f) << "slot " << s + 1 << " is folded into slot 1";

	const auto features = synthetic::featuresOf (stems);
	std::string letters;
	for (const auto& bar : sections::classify (sections::barLevels (features, synthetic::bpm, synthetic::firstBeat, sections::oneMixRoles)))
		letters += bar.section == Section::breakdown ? 'B' : '-';
	EXPECT_EQ (letters, "--------BBBB----") << "the mix gone quiet is the breakdown";
}

TEST (Sections, FoldingSumsTheSlotsSampleBySample)
{
	std::array<std::vector<float>, sections::numStems> stems { std::vector<float> { 0.5f, 0.0f },
															   std::vector<float> {},
															   std::vector<float> { 0.25f, -0.5f },
															   std::vector<float> { 0.0f, 0.25f, 1.0f } };
	sections::foldIntoOneMix (stems);
	EXPECT_EQ (stems[0], (std::vector<float> { 0.75f, -0.25f, 1.0f }));
	EXPECT_EQ (stems[1], (std::vector<float> { 0.0f, 0.0f, 0.0f })) << "every slot as long as the mix";
	EXPECT_EQ (stems[3], (std::vector<float> { 0.0f, 0.0f, 0.0f }));
	EXPECT_EQ (sections::oneMixRoles.drums, 0);
	EXPECT_EQ (sections::oneMixRoles.bass, 0);
}
