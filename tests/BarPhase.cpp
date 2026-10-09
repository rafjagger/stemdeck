#include <gtest/gtest.h>

#include "BarPhase.h"

#include <cmath>
#include <random>

using BarPhase::Stems;

namespace
{
	constexpr double rate = 11025.0;
	constexpr double bpm = 120.0;            // a beat is 0.5 s, a bar 2 s
	constexpr double beat = 60.0 / bpm;
	constexpr double pi = 3.14159265358979323846;

	// The grid's first beat, 0.25 s, is beat 3 of its bar: the first downbeat
	// is two beats later. Taking the first beat as the one gets it wrong.
	constexpr double firstGridBeat = 0.25;
	constexpr double firstDownbeat = firstGridBeat + 2 * beat;
	constexpr int numBars = 40;
	constexpr double length = firstDownbeat + numBars * 4 * beat + 1.0;

	double barStart (int bar) { return firstDownbeat + bar * 4 * beat; }
	double beatTime (int bar, int beatInBar) { return barStart (bar) + beatInBar * beat; }

	std::vector<float> silence() { return std::vector<float> ((size_t) (length * rate), 0.0f); }

	// A decaying tone (bass note, kick, chord voice) added in at `at`.
	void addTone (std::vector<float>& s, double at, double frequency, double seconds, double amplitude, double decay)
	{
		const auto from = (size_t) (at * rate);
		for (size_t i = 0; i < (size_t) (seconds * rate) && from + i < s.size(); ++i)
		{
			const auto t = (double) i / rate;
			s[from + i] += (float) (amplitude * std::exp (-t / decay) * std::sin (2.0 * pi * frequency * t));
		}
	}

	// Decaying high noise (a hat, a crash): white noise differenced, so most
	// of it sits at the top of the spectrum.
	void addNoise (std::vector<float>& s, double at, double seconds, double amplitude, double decay, unsigned seed)
	{
		std::mt19937 random (seed);
		std::uniform_real_distribution<float> white (-1.0f, 1.0f);
		const auto from = (size_t) (at * rate);
		float previous = 0.0f;
		for (size_t i = 0; i < (size_t) (seconds * rate) && from + i < s.size(); ++i)
		{
			const auto t = (double) i / rate;
			const auto w = white (random);
			s[from + i] += (float) (amplitude * std::exp (-t / decay)) * 0.5f * (w - previous);
			previous = w;
		}
	}

	void addKick (std::vector<float>& s, double at) { addTone (s, at, 55.0, 0.25, 0.8, 0.08); }

	// A minor chord (root, minor third, fifth), struck at `at`.
	void addChord (std::vector<float>& s, double at, double root, double seconds)
	{
		for (const auto semitones : { 0, 3, 7, 12 })
			addTone (s, at, root * std::pow (2.0, semitones / 12.0), seconds, 0.15, 0.4);
	}

	Stems stemsOf (std::vector<float> drums, std::vector<float> bass, std::vector<float> other, std::vector<float> vocals = {})
	{
		Stems stems;
		stems.sampleRate = rate;
		stems.mono[BarPhase::drums] = std::move (drums);
		stems.mono[BarPhase::bass] = std::move (bass);
		stems.mono[BarPhase::other] = std::move (other);
		stems.mono[BarPhase::vocals] = std::move (vocals);
		return stems;
	}

	double detectedDownbeat (const Stems& stems, double beatPhase = firstGridBeat)
	{
		return BarPhase::find (stems, bpm, beatPhase, BarPhase::Beats::ontoTheKick).firstDownbeat;
	}
}

TEST (BarPhase, TheFirstDownbeatIsWithinTheFirstBar)
{
	EXPECT_NEAR (BarPhase::firstDownbeat (bpm, 0.25, 0), 0.25, 1e-9);
	EXPECT_NEAR (BarPhase::firstDownbeat (bpm, 0.25, 2), 1.25, 1e-9);
	EXPECT_NEAR (BarPhase::firstDownbeat (bpm, 10.25, 3), 1.75, 1e-9) << "any beat of the grid gives the same bar";
	EXPECT_NEAR (BarPhase::firstDownbeat (bpm, -0.25, 1), 0.75, 1e-9) << "beat 0 is the earliest at or after 0 s";
}

TEST (BarPhase, SilenceIsNoEvidenceAndNoCrash)
{
	const auto result = BarPhase::find (stemsOf ({}, {}, {}), bpm, firstGridBeat, BarPhase::Beats::ontoTheKick);
	EXPECT_EQ (result.offset, 0);
	EXPECT_NEAR (result.firstDownbeat, firstGridBeat, 1e-9) << "without evidence the grid stays as it was";
}

TEST (BarPhase, ABassNoteOnEveryOne)
{
	auto bass = silence();
	for (int bar = 0; bar < numBars; ++bar)
		addTone (bass, barStart (bar), 55.0, 1.2 * beat, 0.6, 0.4);

	EXPECT_NEAR (detectedDownbeat (stemsOf ({}, std::move (bass), {})), firstDownbeat, 1e-6);
}

TEST (BarPhase, AChordChangeOnEveryOne)
{
	// The chord is struck on every beat alike; only its change marks the bar.
	auto other = silence();
	const std::array<double, 4> roots { 220.0, 174.6, 261.6, 196.0 };
	for (int bar = 0; bar < numBars; ++bar)
		for (int b = 0; b < 4; ++b)
			addChord (other, beatTime (bar, b), roots[(size_t) (bar % 4)], beat);
	// The pickup before the first bar: the last chord of the cycle.
	for (const auto at : { firstGridBeat, firstGridBeat + beat })
		addChord (other, at, roots[3], beat);

	EXPECT_NEAR (detectedDownbeat (stemsOf ({}, {}, std::move (other))), firstDownbeat, 1e-6);
}

TEST (BarPhase, AKickOnEveryBeatAndACrashOnTheOne)
{
	auto drums = silence();
	for (double at = firstGridBeat; at < length - beat; at += beat)
	{
		addKick (drums, at);
		addNoise (drums, at + beat / 2, 0.05, 0.3, 0.02, (unsigned) (at * 100));   // the off-beat hat
	}
	for (int bar = 0; bar < numBars; bar += 4)
		addNoise (drums, barStart (bar), 1.5, 0.5, 0.6, 7u + (unsigned) bar);

	EXPECT_NEAR (detectedDownbeat (stemsOf (std::move (drums), {}, {})), firstDownbeat, 1e-6);
}

TEST (BarPhase, DrumsComingBackAfterABreakComeBackOnAOne)
{
	// The kick starts on the grid's first beat, a pickup; then eight bars
	// without drums, and they come back on a one.
	auto drums = silence();
	for (double at = firstGridBeat; at < barStart (16) - 0.01; at += beat)
		addKick (drums, at);
	for (double at = barStart (24); at < length - beat; at += beat)
		addKick (drums, at);

	// A pad that never changes: no evidence either way.
	auto other = silence();
	addTone (other, 0.0, 330.0, length, 0.1, 1.0e9);

	EXPECT_NEAR (detectedDownbeat (stemsOf (std::move (drums), {}, std::move (other))), firstDownbeat, 1e-6);
}

TEST (BarPhase, ATrackThatStartsOnBeatThree)
{
	auto drums = silence(), bass = silence(), other = silence(), vocals = silence();
	const std::array<double, 4> roots { 220.0, 174.6, 261.6, 196.0 };

	for (double at = firstGridBeat; at < length - beat; at += beat)
		addKick (drums, at);
	for (int bar = 0; bar < numBars; ++bar)
	{
		if (bar % 8 == 0)
			addNoise (drums, barStart (bar), 1.5, 0.5, 0.6, 11u + (unsigned) bar);
		addTone (bass, barStart (bar), 55.0 * roots[(size_t) (bar % 4)] / 220.0, 0.9 * beat, 0.6, 0.3);
		addTone (bass, beatTime (bar, 2) + beat / 2, 55.0, 0.4 * beat, 0.3, 0.1);
		addChord (other, barStart (bar), roots[(size_t) (bar % 4)], 4 * beat);
	}
	// A voice that starts its phrase a beat early, as voices do: not evidence of the one.
	for (int bar = 4; bar < numBars; bar += 4)
		addTone (vocals, beatTime (bar - 1, 3), 440.0, 2 * beat, 0.2, 2.0);

	const auto stems = stemsOf (std::move (drums), std::move (bass), std::move (other), std::move (vocals));
	EXPECT_NEAR (detectedDownbeat (stems), firstDownbeat, 1e-6);
	EXPECT_NEAR (detectedDownbeat (stems, firstGridBeat + 37 * beat), firstDownbeat, 1e-6)
		<< "named by a later beat, the grid is the same";
}

namespace
{
	// Four to the floor: the kick on every beat, a loud open hat between.
	Stems fourToTheFloor()
	{
		auto drums = silence();
		for (double at = firstGridBeat; at < length - beat; at += beat)
		{
			addKick (drums, at);
			addNoise (drums, at + beat / 2, 0.2, 0.9, 0.08, (unsigned) (at * 100));
		}
		for (int bar = 0; bar < numBars; bar += 8)
			addNoise (drums, barStart (bar), 1.5, 0.5, 0.6, 7u + (unsigned) bar);
		// Drums out for two bars and back on a one.
		auto quiet = [&drums] (double from, double to)
		{
			std::fill (drums.begin() + (long) (from * rate), drums.begin() + (long) (to * rate), 0.0f);
		};
		quiet (barStart (16) - 0.01, barStart (18) - 0.01);
		return stemsOf (std::move (drums), {}, {});
	}
}

TEST (BarPhase, BeatsOnTheOffBeatMoveOntoTheKick)
{
	// Beats locked onto the hats, half a beat after the kick.
	const auto result = BarPhase::find (fourToTheFloor(), bpm, firstGridBeat + beat / 2, BarPhase::Beats::ontoTheKick);
	EXPECT_TRUE (result.movedHalfABeat);
	EXPECT_NEAR (result.firstDownbeat, firstDownbeat, 1e-6);
}

TEST (BarPhase, BeatsOnTheKickStay)
{
	const auto result = BarPhase::find (fourToTheFloor(), bpm, firstGridBeat, BarPhase::Beats::ontoTheKick);
	EXPECT_FALSE (result.movedHalfABeat);
	EXPECT_NEAR (result.firstDownbeat, firstDownbeat, 1e-6);
}

TEST (BarPhase, KeptBeatsAreNeverMoved)
{
	// A grid already in use keeps its beats: only its one is found again.
	const auto offBeat = firstGridBeat + beat / 2;
	const auto result = BarPhase::find (fourToTheFloor(), bpm, offBeat, BarPhase::Beats::keep);
	EXPECT_FALSE (result.movedHalfABeat);
	const auto beatsFromGrid = (result.firstDownbeat - offBeat) / beat;
	EXPECT_NEAR (beatsFromGrid, std::round (beatsFromGrid), 1e-9);
}

TEST (BarPhase, ACachedGridOnTheOffBeatMovesOntoTheKick)
{
	const auto found = BarPhase::foundAgain (fourToTheFloor(), bpm, firstGridBeat + beat / 2);
	EXPECT_NEAR (found, firstDownbeat, 1e-6) << "half a beat onto the kick, then the one";
}

TEST (BarPhase, ACachedGridOnTheKickKeepsItsBeats)
{
	const auto found = BarPhase::foundAgain (fourToTheFloor(), bpm, firstGridBeat);
	EXPECT_NEAR (found, firstDownbeat, 1e-6);
}
