// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "StemDeckPlayer.h"

#include <chrono>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <thread>

// KEY quality in the player: a new engine means new stretchers, started
// again. Where nobody hears them start (KEY off, or the deck stopped) they
// are swapped at once; under a playing KEY they are built beside the
// running ones and taken at the next track -- a load, or CONTINUE's change.
namespace
{
constexpr double rate = 44100.0;
constexpr int block = 256;
constexpr double pi = 3.14159265358979323846;
constexpr double setSeconds = 1.5;
using keylock::Engine;

void writeToneStem (const juce::File& file, double hz, double seconds)
{
	const auto length = (int) std::lround (seconds * rate);
	juce::AudioBuffer<float> audio (2, length);
	for (int ch = 0; ch < 2; ++ch)
		for (int i = 0; i < length; ++i)
			audio.setSample (ch, i, 0.3f * (float) std::sin (2.0 * pi * hz * (i + 0.25) / rate));

	juce::FlacAudioFormat flac;
	std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
	auto writer = stream == nullptr ? nullptr
	                                : flac.createWriterFor (stream, juce::AudioFormatWriterOptions{}
	                                                                    .withSampleRate (rate)
	                                                                    .withNumChannels (2)
	                                                                    .withBitsPerSample (24));
	if (writer == nullptr || ! writer->writeFromAudioSampleBuffer (audio, 0, length))
		throw std::runtime_error ("cannot write " + file.getFullPathName().toStdString());
}

bool waitFor (const std::function<bool()>& condition, double seconds = 10.0)
{
	const auto until = std::chrono::steady_clock::now() + std::chrono::duration<double> (seconds);
	while (! condition())
	{
		if (std::chrono::steady_clock::now() > until)
			return false;
		std::this_thread::sleep_for (std::chrono::milliseconds (2));
	}
	return true;
}

class PlayerKeyLockEngine : public ::testing::Test
{
protected:
	void SetUp() override
	{
		ASSERT_TRUE (folder.createDirectory().wasOk());
		formats.registerBasicFormats();
		first = makeSet ("first", 441.0);
		second = makeSet ("second", 661.5);
		player.prepareToPlay (block, rate);
	}

	void TearDown() override { folder.deleteRecursively(); }

	StemSet makeSet (const juce::String& name, double hz)
	{
		StemSet set;
		set.name = name;
		for (int s = 0; s < StemSet::numStems; ++s)
		{
			set.files[(size_t) s] = folder.getChildFile (name + "-0" + juce::String (s + 1) + ".flac");
			set.stemNames[(size_t) s] = "stem" + juce::String (s + 1);
			writeToneStem (set.files[(size_t) s], hz, setSeconds);
		}
		set.lengthSeconds = setSeconds;
		return set;
	}

	void loadReady (const StemSet& set)
	{
		ASSERT_EQ (player.load (set), juce::String());
		ASSERT_TRUE (waitFor ([this] { return player.isReady(); }));
	}

	// Plays `seconds`; the loudest sample of stem 0's left side.
	float play (double seconds)
	{
		juce::AudioBuffer<float> out (StemDeckPlayer::numOutputChannels, block);
		float peak = 0.0f;
		for (int i = 0; i < (int) std::ceil (seconds * rate / block); ++i)
		{
			out.clear();
			player.getNextAudioBlock (juce::AudioSourceChannelInfo (&out, 0, block));
			peak = juce::jmax (peak, out.getMagnitude (0, 0, block));
		}
		return peak;
	}

	juce::File folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("stemdeck-keyengine", "");
	juce::AudioFormatManager formats;
	StemDeckPlayer player { formats };
	StemSet first, second;
};
}

TEST_F (PlayerKeyLockEngine, KeyLockBuildsTheChosenEngine)
{
	loadReady (first);
	EXPECT_EQ (player.keyLockEngine(), Engine::standard) << "the default";
	EXPECT_FALSE (player.keyLockEngineInUse().has_value()) << "no stretchers before KEY";

	player.setKeyLockEngine (Engine::high);
	EXPECT_FALSE (player.keyLockEngineInUse().has_value()) << "still built only by KEY";
	player.setKeyLock (true);
	EXPECT_EQ (player.keyLockEngineInUse(), Engine::high);

	player.play();
	EXPECT_GT (play (0.5), 0.2f) << "the high engine plays";
}

TEST_F (PlayerKeyLockEngine, StoppedTheNewEngineComesAtOnce)
{
	loadReady (first);
	player.setKeyLock (true);
	ASSERT_EQ (player.keyLockEngineInUse(), Engine::standard);

	player.setKeyLockEngine (Engine::high);
	EXPECT_EQ (player.keyLockEngineInUse(), Engine::high);
	EXPECT_FALSE (player.hasStandbyStretchers());
}

TEST_F (PlayerKeyLockEngine, WithKeyOffTheNewEngineComesAtOnceWhilePlaying)
{
	loadReady (first);
	player.setKeyLock (true);
	player.setKeyLock (false);
	player.play();
	play (0.2);

	player.setKeyLockEngine (Engine::high);
	EXPECT_EQ (player.keyLockEngineInUse(), Engine::high);
}

TEST_F (PlayerKeyLockEngine, UnderAPlayingKeyTheNewEngineWaitsForTheNextLoad)
{
	loadReady (first);
	player.setKeyLock (true);
	player.play();
	play (0.2);

	player.setKeyLockEngine (Engine::high);
	EXPECT_EQ (player.keyLockEngine(), Engine::high);
	EXPECT_EQ (player.keyLockEngineInUse(), Engine::standard) << "not swapped under the ear";
	ASSERT_TRUE (waitFor ([this] { return player.hasStandbyStretchers(); })) << "built beside, off the audio thread";
	EXPECT_GT (play (0.3), 0.2f) << "the running ones play on";
	EXPECT_EQ (player.keyLockEngineInUse(), Engine::standard);

	loadReady (second);
	EXPECT_EQ (player.keyLockEngineInUse(), Engine::high);
	EXPECT_FALSE (player.hasStandbyStretchers());
	player.play();
	EXPECT_GT (play (0.5), 0.2f);
}

TEST_F (PlayerKeyLockEngine, ChosenBackBeforeTheLoadNothingChanges)
{
	loadReady (first);
	player.setKeyLock (true);
	player.play();
	play (0.2);

	player.setKeyLockEngine (Engine::high);
	player.setKeyLockEngine (Engine::standard);
	EXPECT_FALSE (player.hasStandbyStretchers());
	loadReady (second);
	EXPECT_EQ (player.keyLockEngineInUse(), Engine::standard);
}

// CONTINUE's change is the next track too: the audio thread takes the
// standby stretchers (a pointer swap) and the played-out ones go with the
// played-out set, freed off the audio thread.
TEST_F (PlayerKeyLockEngine, ContinuesChangeTakesTheStandbyEngine)
{
	loadReady (first);
	player.setKeyLock (true);
	player.setContinue (true);
	ASSERT_EQ (player.stageNext (second), juce::String());
	ASSERT_TRUE (waitFor ([this] { return player.isStagedReady(); }));
	player.play();
	play (0.2);

	player.setKeyLockEngine (Engine::high);
	ASSERT_TRUE (waitFor ([this] { return player.hasStandbyStretchers(); }));
	play (setSeconds);

	ASSERT_EQ (player.handovers(), 1);
	EXPECT_EQ (player.keyLockEngineInUse(), Engine::high);
	EXPECT_FALSE (player.hasStandbyStretchers());
	EXPECT_GT (play (0.3), 0.2f) << "the second set plays on the new engine";
	player.releaseRetired();
}
