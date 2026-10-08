#include <gtest/gtest.h>

#include <cmath>

#include "Sections.h"
#include "SyntheticStems.h"

TEST (SectionFeatures, AttacksAreCounted)
{
	const auto features = synthetic::featuresOf (synthetic::fromScript ("qqqqssss"));
	const auto bars = sections::barLevels (features, synthetic::bpm, synthetic::firstBeat, {});
	ASSERT_EQ (bars.size(), 8u);
	EXPECT_NEAR (bars[1].drumOnsetsPerBeat, 1.0f, 0.25f) << "a snare on every beat";
	EXPECT_NEAR (bars[6].drumOnsetsPerBeat, 4.0f, 0.5f) << "a snare on every sixteenth";
}

TEST (SectionFeatures, TheSameAnswerWhateverTheBlockSize)
{
	const auto stems = synthetic::fromScript ("FFFFqqss");
	const auto a = synthetic::featuresOf (stems, 4096);
	const auto b = synthetic::featuresOf (stems, 37);
	ASSERT_EQ (a.frames.size(), b.frames.size());
	for (size_t f = 0; f < a.frames.size(); ++f)
		EXPECT_EQ (a.frames[f].onsets, b.frames[f].onsets) << "frame " << f;
}

TEST (SectionFeatures, BarsCountFromTheDownbeat)
{
	// Nine frames of a beat; the downbeat on frame 1: frame 0 is a pickup.
	sections::Features features;
	features.hopSeconds = 0.5;
	for (int f = 0; f < 9; ++f)
	{
		sections::Frame frame;
		frame.rms[0] = (float) f;
		features.frames.push_back (frame);
	}

	const auto bars = sections::barLevels (features, 120.0, 0.5, {});
	ASSERT_EQ (bars.size(), 2u);
	EXPECT_FLOAT_EQ (bars[0].drums, 2.5f) << "frames 1-4";
	EXPECT_FLOAT_EQ (bars[1].drums, 6.5f) << "frames 5-8";

	const auto oneLater = sections::barLevels (features, 120.0, 1.0, {});
	EXPECT_FLOAT_EQ (oneLater[0].drums, 3.5f) << "the one a beat later (1>): frames 2-5";
}

TEST (SectionFeatures, NoGridNoBars)
{
	EXPECT_TRUE (sections::barLevels (synthetic::featuresOf (synthetic::fromScript ("FF")), 0.0, 0.0, {}).empty());
}

TEST (SectionFeatures, TheCacheTextComesBack)
{
	const auto features = synthetic::featuresOf (synthetic::fromScript ("Fq"));
	const auto back = sections::decode (sections::encode (features));
	ASSERT_TRUE (back.has_value());
	EXPECT_DOUBLE_EQ (back->startSeconds, features.startSeconds);
	EXPECT_DOUBLE_EQ (back->hopSeconds, features.hopSeconds);
	ASSERT_EQ (back->frames.size(), features.frames.size());
	for (size_t f = 0; f < features.frames.size(); ++f)
	{
		for (size_t s = 0; s < 4; ++s)
			EXPECT_NEAR (back->frames[f].rms[s], features.frames[f].rms[s], 1e-6f);
		EXPECT_EQ (back->frames[f].onsets, features.frames[f].onsets);
	}
}

TEST (SectionFeatures, ACacheFileThatIsNotOneIsNothing)
{
	EXPECT_FALSE (sections::decode ("").has_value());
	EXPECT_FALSE (sections::decode ("a3-sections 2 0 0.5 0\n").has_value()) << "another version";
	EXPECT_FALSE (sections::decode ("a3-sections 1 0 0.5 3\n1 2 3\n").has_value()) << "cut short";
	EXPECT_FALSE (sections::decode ("<ANALYSIS/>").has_value());
}

// A corrupt cache file must not make the message thread allocate what it claims.
TEST (SectionFeatures, ACountTheBodyCannotHoldIsNothing)
{
	std::optional<sections::Features> decoded;
	EXPECT_NO_THROW (decoded = sections::decode ("a3-sections 1 0 0.5 1000000000000000\n1 2 3 4 5 6 7 8\n"));
	EXPECT_FALSE (decoded.has_value());
}

TEST (SectionFeatures, AnEmptySetWithoutANewlineComesBack)
{
	const auto decoded = sections::decode ("a3-sections 1 0 0.5 0");
	ASSERT_TRUE (decoded.has_value());
	EXPECT_TRUE (decoded->frames.empty());
}

TEST (SectionFeatures, MoreFramesThanAnySetIsNothing)
{
	const auto frames = sections::maxCachedFrames + 1;
	std::string text = "a3-sections 1 0 0.5 " + std::to_string (frames) + "\n";
	for (size_t i = 0; i < frames; ++i)
		text += "0 0 0 0 0 0 0 0\n";
	EXPECT_FALSE (sections::decode (text).has_value());
}

// Sets whose stems are not in the stem creator's order must still find drums and bass.
TEST (SectionFeatures, TheStemsByNameElseByOrder)
{
	const auto demucs = sections::rolesFor ({ "drums", "bass", "other", "vocals" });
	EXPECT_EQ (demucs.drums, 0);
	EXPECT_EQ (demucs.bass, 1);

	const auto numbered = sections::rolesFor ({ "001", "002", "003", "004" });
	EXPECT_EQ (numbered.drums, 0);
	EXPECT_EQ (numbered.bass, 1);

	const auto named = sections::rolesFor ({ "DUB", "KICK", "PADS", "PERC" });
	EXPECT_EQ (named.drums, 1) << "KICK";
	EXPECT_EQ (named.bass, 0) << "the order's bass is taken: the next free stem";

	const auto shuffled = sections::rolesFor ({ "Vocals", "Bass", "Drums", "Other" });
	EXPECT_EQ (shuffled.drums, 2);
	EXPECT_EQ (shuffled.bass, 1);
}

// Real analysed grids never put the downbeat on a 10 ms slice boundary.
TEST (SectionFeatures, TheKickOnAnOffSliceDownbeatCounts)
{
	const double downbeat = 0.2537;
	synthetic::Stems stems;
	const auto total = (size_t) ((downbeat + 2.0) * synthetic::sampleRate);
	for (auto& stem : stems.samples)
		stem.assign (total, 0.0f);
	uint32_t state = 1;
	synthetic::hit (stems.samples[0], downbeat, 60.0, 0.8f, 0.08, 0.15, state);

	sections::FeatureBuilder builder (synthetic::sampleRate, downbeat, synthetic::beat);
	builder.add ({ stems.samples[0].data(), stems.samples[1].data(), stems.samples[2].data(), stems.samples[3].data() }, (int) total);
	const auto bars = sections::barLevels (builder.finish(), synthetic::bpm, downbeat, {});
	ASSERT_EQ (bars.size(), 1u);
	EXPECT_GT (bars[0].drumOnsetsPerBeat, 0.0f) << "the kick on the one";
}

TEST (SectionFeatures, NoFiniteGridNoBars)
{
	const auto features = synthetic::featuresOf (synthetic::fromScript ("FF"));
	const double nan = std::nan ("");
	EXPECT_TRUE (sections::barLevels (features, nan, 0.25, {}).empty());
	EXPECT_TRUE (sections::barLevels (features, 120.0, nan, {}).empty());
	EXPECT_TRUE (sections::barLevels (features, INFINITY, 0.25, {}).empty());
	auto badHop = features;
	badHop.hopSeconds = nan;
	EXPECT_TRUE (sections::barLevels (badHop, 120.0, 0.25, {}).empty());
}
