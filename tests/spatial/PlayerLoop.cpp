// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "StemDeckPlayer.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

// A loop played as a CDJ plays it: the seam is a cut at the sample, with at
// most a few milliseconds of both sides crossfaded -- no fade out and in, no
// dip -- and the playhead never runs past the loop's end, however many passes
// and whatever tempo, with key lock on or off.
namespace
{
constexpr double rate = 44100.0;
constexpr int block = 256;
// 102.4 samples a period, on a bin of Rubber Band's 2048-point analysis:
// a tone between two bins comes out of a phase vocoder beating by about
// 2 dB, seam or not. Even so, away from rate 1 the stretcher swings a steady
// tone by 3 dB every 2048 samples; a window of 4096 (40 whole periods, two
// of those swings) reads it steady.
constexpr double toneHz = 44100.0 / 2048.0 * 20.0;
constexpr float amplitude = 0.5f;
constexpr int window = 4096, hop = 256;
constexpr double trackSeconds = 40.0;
constexpr double pi = 3.14159265358979323846;

void writeStem (const juce::File& file, bool tone)
{
	const auto length = (int) (trackSeconds * rate);
	juce::AudioBuffer<float> audio (2, length);
	audio.clear();
	if (tone)
		for (int i = 0; i < length; ++i)
		{
			const auto v = amplitude * (float) std::sin (2.0 * pi * toneHz * i / rate);
			audio.setSample (0, i, v);
			audio.setSample (1, i, v);
		}

	juce::WavAudioFormat wav;
	std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
	auto writer = stream == nullptr ? nullptr
	                                : wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}
	                                                                   .withSampleRate (rate)
	                                                                   .withNumChannels (2)
	                                                                   .withBitsPerSample (32));
	if (writer == nullptr || ! writer->writeFromAudioSampleBuffer (audio, 0, length))
		throw std::runtime_error ("cannot write " + file.getFullPathName().toStdString());
}

struct Render
{
	std::vector<float> left;      // stem 0, left
	int wraps = 0;
	double furthest = 0.0;        // the playhead's furthest point
	double earliest = 1.0e9;      // and its earliest
};

struct Case
{
	double loopSeconds;
	double speed;
	bool keyLock;
};

std::string name (const Case& c)
{
	return "loop " + std::to_string (c.loopSeconds) + " s, speed " + std::to_string (c.speed) + (c.keyLock ? ", key lock" : "");
}

void PrintTo (const Case& c, std::ostream* os) { *os << name (c); }

class PlayerLoop : public ::testing::TestWithParam<Case>
{
protected:
	void SetUp() override
	{
		ASSERT_TRUE (folder.createDirectory().wasOk());
		formats.registerBasicFormats();
		set.name = "tone";
		for (int s = 0; s < StemSet::numStems; ++s)
		{
			set.files[(size_t) s] = folder.getChildFile ("stem" + juce::String (s) + ".wav");
			set.stemNames[(size_t) s] = "stem" + juce::String (s);
			writeStem (set.files[(size_t) s], s == 0);
		}
		ASSERT_EQ (player.load (set), juce::String());
		player.prepareToPlay (block, rate);
	}

	void TearDown() override { folder.deleteRecursively(); }

	// `passes` passes of the loop, played from its start.
	Render play (double loopStart, double loopEnd, int passes)
	{
		Render r;
		juce::AudioBuffer<float> buffer (StemDeckPlayer::numOutputChannels, block);
		const auto seconds = (loopEnd - loopStart) * passes / player.getSpeed() + 0.5;
		const auto blocks = (int) (seconds * rate / block);
		auto last = player.getPosition();

		for (int b = 0; b < blocks; ++b)
		{
			buffer.clear();
			player.getNextAudioBlock ({ &buffer, 0, block });
			r.left.insert (r.left.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + block);

			const auto now = player.getPosition();
			if (now < last - 0.5 * (loopEnd - loopStart))
				++r.wraps;
			r.furthest = std::max (r.furthest, now);
			r.earliest = std::min (r.earliest, now);
			last = now;
		}
		return r;
	}

	juce::File folder { juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("stemdeck-loop", "") };
	juce::AudioFormatManager formats;
	StemSet set;
	StemDeckPlayer player { formats };
};

double rmsDb (const std::vector<float>& x, size_t from)
{
	double sum = 0.0;
	for (size_t i = from; i < from + window; ++i)
		sum += (double) x[i] * x[i];
	return 10.0 * std::log10 (sum / window + 1e-30);
}
}

TEST_P (PlayerLoop, SeamlessAndNeverPastItsEnd)
{
	const auto c = GetParam();
	player.setSpeed (c.speed);
	player.setKeyLock (c.keyLock);

	// Half a period off the tone's: the seam joins it to itself upside down,
	// the hardest case for a cut and for a crossfade.
	const auto period = rate / toneHz;
	auto loopSamples = std::round (c.loopSeconds * rate);
	loopSamples += std::round (std::fmod (1.5 * period - std::fmod (loopSamples, period), period));
	const auto start = 3.0 + 13.0 / rate;
	const auto end = start + loopSamples / rate;
	player.setLoop (start, end);
	player.play();
	constexpr int passes = 24;
	const auto r = play (start, end, passes);

	EXPECT_GE (r.wraps, passes - 1) << name (c);
	EXPECT_LE (r.furthest, end + 1.0e-6) << name (c) << ": the playhead ran past the loop's end";
	EXPECT_GE (r.earliest, start - 0.01) << name (c);
	EXPECT_TRUE (player.hasLoop());

	// From the second pass on: every window of the output (seams included)
	// within 0.5 dB of the body's level -- the median window. Away from rate
	// 1 the stretcher holds a steady tone about 0.6 dB under the file's own
	// and starts at the file's (each seam starts it again), so no window may
	// rise above the louder of the two.
	const auto settled = (size_t) (0.3 * rate);
	std::vector<double> levels;
	for (size_t i = settled; i + window < r.left.size(); i += hop)
		levels.push_back (rmsDb (r.left, i));
	ASSERT_FALSE (levels.empty());
	auto sorted = levels;
	std::nth_element (sorted.begin(), sorted.begin() + (long) sorted.size() / 2, sorted.end());
	const auto bodyDb = sorted[sorted.size() / 2];
	const auto [lowest, highest] = std::minmax_element (levels.begin(), levels.end());
	const auto toneDb = 20.0 * std::log10 (amplitude / std::sqrt (2.0));
	EXPECT_GT (*lowest, bodyDb - 0.5) << name (c) << ": a dip at the seam";
	EXPECT_LT (*highest, std::max (bodyDb, toneDb) + 0.5) << name (c) << ": a bump at the seam";
	EXPECT_NEAR (bodyDb, toneDb, 1.0) << name (c) << ": the tone's own level";

	// No step between samples beyond twice the tone's own steepest.
	const auto heardHz = toneHz * (c.keyLock ? 1.0 : c.speed);
	const auto ownStep = amplitude * 2.0 * pi * heardHz / rate;
	double steepest = 0.0;
	for (size_t i = settled; i < r.left.size(); ++i)
		steepest = std::max (steepest, (double) std::abs (r.left[i] - r.left[i - 1]));
	EXPECT_LT (steepest, 2.0 * ownStep) << name (c) << ": a click at the seam";
}

INSTANTIATE_TEST_SUITE_P (LengthsTemposAndKeyLock, PlayerLoop,
	::testing::Values (Case { 0.25, 1.0, false }, Case { 0.5, 1.0, false }, Case { 1.0, 1.0, false },
					   Case { 0.5, 0.94, false }, Case { 0.5, 1.08, false },
					   Case { 0.25, 1.0, true }, Case { 0.5, 1.0, true }, Case { 1.0, 1.0, true },
					   Case { 0.5, 0.94, true }, Case { 0.5, 1.08, true }));

namespace
{
class PlayerLoopSeek : public PlayerLoop {};
}

// A jump from elsewhere while looping -- the sync's phase jump, a jog search
// -- lands in the loop, never behind its end: the loop the screen shows is
// the loop that plays.
TEST_P (PlayerLoopSeek, AJumpPastTheEndStaysInTheLoop)
{
	const auto c = GetParam();
	player.setSpeed (c.speed);
	player.setKeyLock (c.keyLock);
	const auto start = 3.0, end = start + c.loopSeconds;
	player.setLoop (start, end);
	player.play();
	play (start, end, 2);

	player.setPosition (end + 0.3 * c.loopSeconds);
	EXPECT_GE (player.getPosition(), start);
	EXPECT_LT (player.getPosition(), end);

	const auto r = play (start, end, 6);
	EXPECT_LE (r.furthest, end + 1.0e-6) << name (c);
	EXPECT_GE (r.wraps, 4) << name (c);
}

INSTANTIATE_TEST_SUITE_P (KeyLockOnAndOff, PlayerLoopSeek,
	::testing::Values (Case { 0.5, 1.0, false }, Case { 0.5, 1.03, true }));

