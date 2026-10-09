#include <gtest/gtest.h>

#include "StemHandover.h"

#include <cmath>
#include <vector>

using namespace StemHandover;

namespace
{
	constexpr double bar = 2.0;   // 120 BPM

	// A stem at `level` throughout `seconds`, silent where `silent` says so.
	Envelope steady (double seconds, float level = 0.5f)
	{
		Envelope e;
		e.rms.assign ((size_t) std::lround (seconds / e.hop), level);
		return e;
	}

	void silence (Envelope& e, double from, double to)
	{
		for (auto i = (size_t) std::lround (from / e.hop); i < (size_t) std::lround (to / e.hop) && i < e.rms.size(); ++i)
			e.rms[i] = 0.0f;
	}

	// Downbeats 0..bars of an overlap that starts at `oldStart` in the old
	// track and at 0 in the new one, both at 120 BPM.
	std::vector<Downbeat> downbeats (int bars, double oldStart)
	{
		std::vector<Downbeat> out;
		for (int b = 0; b <= bars; ++b)
			out.push_back ({ b, oldStart + b * bar, b * bar });
		return out;
	}

	Envelopes steadyStems (double seconds)
	{
		return { steady (seconds), steady (seconds), steady (seconds), steady (seconds) };
	}
}

TEST (StemHandover, TheEnvelopeIsTheRmsPerHop)
{
	// One second of a full-scale square wave, then one of silence, at 1 kHz.
	const double rate = 1000.0;
	std::vector<float> left (2000, 0.0f), right (2000, 0.0f);
	for (size_t i = 0; i < 1000; ++i)
		left[i] = right[i] = (i % 2 == 0) ? 1.0f : -1.0f;

	EnvelopeBuilder builder (rate);
	const float* channels[] = { left.data(), right.data() };
	builder.add (channels, 2, 700);   // blocks that do not line up with the hops
	const float* rest[] = { left.data() + 700, right.data() + 700 };
	builder.add (rest, 2, 1300);
	const auto e = builder.finish();

	ASSERT_EQ (e.rms.size(), (size_t) std::lround (2.0 / e.hop));
	EXPECT_NEAR (e.levelAt (0.5), 1.0f, 1e-6);
	EXPECT_NEAR (e.levelAt (1.5), 0.0f, 1e-6);
	EXPECT_FLOAT_EQ (e.levelAt (-1.0), 0.0f) << "before the track: silence";
	EXPECT_FLOAT_EQ (e.levelAt (5.0), 0.0f) << "after it, too";
}

TEST (StemHandover, AStemThatPausesOnABarIsSwitchedOnThatBar)
{
	auto old = steady (120.0);
	silence (old, 40.0 + 6 * bar, 40.0 + 7 * bar);   // a bar of rest from downbeat 6
	const auto incoming = steady (120.0);

	auto candidates = downbeats (10, 40.0);
	candidates.erase (candidates.begin());   // not on the drums' downbeat
	EXPECT_EQ (chooseDownbeat (old, incoming, candidates, window), 6);
}

TEST (StemHandover, ANewStemIsBroughtInWhereItsPhraseBegins)
{
	const auto old = steady (120.0);
	auto incoming = steady (120.0);
	silence (incoming, 0.0, 4 * bar);   // the new bass enters on its bar 4

	auto candidates = downbeats (10, 40.0);
	candidates.erase (candidates.begin());
	EXPECT_EQ (chooseDownbeat (old, incoming, candidates, window), 4);
}

TEST (StemHandover, WithNothingToHearBassAndOtherAreSpreadBassFirst)
{
	const auto old = steadyStems (120.0), incoming = steadyStems (120.0);
	const auto pair = chooseBassAndOther (old, incoming, downbeats (10, 40.0), 10, window);
	EXPECT_LT (pair.bass, pair.other) << "bass before other";
	EXPECT_NEAR (pair.bass, 10 / 3.0, 1.0);
	EXPECT_NEAR (pair.other, 2 * 10 / 3.0, 1.0);
}

TEST (StemHandover, BassAndOtherNeverShareADownbeat)
{
	// Both pause on the same bar: one of them takes it, the other moves.
	auto old = steadyStems (120.0);
	silence (old[1], 40.0 + 5 * bar, 40.0 + 6 * bar);
	silence (old[2], 40.0 + 5 * bar, 40.0 + 6 * bar);
	const auto pair = chooseBassAndOther (old, steadyStems (120.0), downbeats (10, 40.0), 10, window);
	EXPECT_NE (pair.bass, pair.other);
	EXPECT_TRUE (pair.bass == 5 || pair.other == 5);
	EXPECT_GE (std::abs (pair.bass - pair.other), 2) << "not bunched";
}

TEST (StemHandover, TheAnalysisMovesEachSwapToItsPause)
{
	auto old = steadyStems (120.0);
	silence (old[1], 40.0 + 3 * bar, 40.0 + 4 * bar);   // bass rests on bar 3
	silence (old[2], 40.0 + 7 * bar, 40.0 + 8 * bar);   // other on bar 7
	const auto pair = chooseBassAndOther (old, steadyStems (120.0), downbeats (10, 40.0), 10, window);
	EXPECT_EQ (pair.bass, 3);
	EXPECT_EQ (pair.other, 7);
}

TEST (StemHandover, NeitherSwapsOnTheDrumsDownbeatNorTheEnd)
{
	auto old = steadyStems (120.0);
	silence (old[1], 40.0, 40.0 + bar);          // pauses right at the start
	silence (old[2], 40.0 + 10 * bar, 120.0);    // and at the end
	const auto pair = chooseBassAndOther (old, steadyStems (120.0), downbeats (10, 40.0), 10, window);
	for (auto b : { pair.bass, pair.other })
	{
		EXPECT_GT (b, 0);
		EXPECT_LT (b, 10);
	}
}

TEST (StemHandover, OtherGoesFirstOnlyWhenTheAnalysisStronglyWantsIt)
{
	// The other stem rests early and the bass late, each clearly.
	auto old = steadyStems (120.0);
	silence (old[2], 40.0 + 2 * bar, 40.0 + 3 * bar);
	silence (old[1], 40.0 + 8 * bar, 40.0 + 9 * bar);
	const auto pair = chooseBassAndOther (old, steadyStems (120.0), downbeats (10, 40.0), 10, window);
	EXPECT_EQ (pair.other, 2);
	EXPECT_EQ (pair.bass, 8);
}

TEST (StemHandover, AShortOverlapStillGivesTwoDownbeats)
{
	const auto old = steadyStems (120.0), incoming = steadyStems (120.0);
	const auto four = chooseBassAndOther (old, incoming, downbeats (4, 40.0), 4, window);
	EXPECT_NE (four.bass, four.other);
	EXPECT_GT (std::min (four.bass, four.other), 0);
	EXPECT_LT (std::max (four.bass, four.other), 4);

	// Too short for both inside it: what does not fit comes with the vocals.
	const auto one = chooseBassAndOther (old, incoming, downbeats (1, 40.0), 1, window);
	EXPECT_LE (one.bass, 1);
	EXPECT_EQ (one.other, 1);
}

// Where a track is heard: from the first sustained sound to the last, the
// silence around it left out.
namespace
{
	Envelopes onlyDrums (const Envelope& drums)
	{
		Envelopes stems;
		stems[0] = drums;
		for (size_t s = 1; s < stems.size(); ++s)
			stems[s].rms.assign (drums.rms.size(), 0.0f);
		return stems;
	}

	void setLevel (Envelope& e, double from, double to, float level)
	{
		for (auto i = (size_t) std::lround (from / e.hop); i < (size_t) std::lround (to / e.hop) && i < e.rms.size(); ++i)
			e.rms[i] = level;
	}

	float db (double decibels) { return (float) std::pow (10.0, decibels / 20.0); }
}

TEST (StemHandover, LeadingAndTrailingSilenceAreNotTheTrack)
{
	auto drums = steady (100.0);
	silence (drums, 0.0, 5.0);
	silence (drums, 90.0, 100.0);
	const auto span = audibleSpan (onlyDrums (drums));
	ASSERT_TRUE (span);
	EXPECT_NEAR (span->start, 5.0, 0.06);
	EXPECT_NEAR (span->end, 90.0, 0.06);
}

TEST (StemHandover, AFadeOutIsHeardUntilFiftyDecibelsDown)
{
	// Steady to 60 s, then a fade of 80 dB over 20 s: 50 dB down at 72.5 s.
	auto drums = steady (100.0, 0.5f);
	for (auto i = (size_t) std::lround (60.0 / drums.hop); i < drums.rms.size(); ++i)
	{
		const auto t = i * drums.hop - 60.0;
		drums.rms[i] = t < 20.0 ? 0.5f * db (-80.0 * t / 20.0) : 0.0f;
	}
	const auto span = audibleSpan (onlyDrums (drums));
	ASSERT_TRUE (span);
	EXPECT_NEAR (span->end, 72.5, 0.2);
}

TEST (StemHandover, AQuietIntroIsNotSilence)
{
	auto drums = steady (100.0, 0.5f);
	setLevel (drums, 0.0, 10.0, 0.5f * db (-30.0));
	const auto span = audibleSpan (onlyDrums (drums));
	ASSERT_TRUE (span);
	EXPECT_NEAR (span->start, 0.0, 0.06);
}

TEST (StemHandover, AClickInTheSilenceDoesNotCount)
{
	auto drums = steady (100.0, 0.5f);
	silence (drums, 0.0, 5.0);
	silence (drums, 90.0, 100.0);
	setLevel (drums, 2.0, 2.05, 0.5f);    // a click before the music
	setLevel (drums, 95.0, 95.1, 0.5f);   // and one after it
	const auto span = audibleSpan (onlyDrums (drums));
	ASSERT_TRUE (span);
	EXPECT_NEAR (span->start, 5.0, 0.06);
	EXPECT_NEAR (span->end, 90.0, 0.06);
}

TEST (StemHandover, AnyStemMakesTheTrackHeard)
{
	auto stems = onlyDrums (steady (100.0));
	silence (stems[0], 0.0, 8.0);
	setLevel (stems[3], 3.0, 100.0, 0.3f);   // the vocals start first
	const auto span = audibleSpan (stems);
	ASSERT_TRUE (span);
	EXPECT_NEAR (span->start, 3.0, 0.06);
}

TEST (StemHandover, SilenceOrNothingHasNoSpan)
{
	EXPECT_FALSE (audibleSpan (Envelopes {}));
	EXPECT_FALSE (audibleSpan (onlyDrums (steady (10.0, 0.0f))));
}
