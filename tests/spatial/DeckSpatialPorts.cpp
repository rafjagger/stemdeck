// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "DeckSpatialPorts.h"
#include "Outputs.h"
#include "StemDeckPlayer.h"
#include "StemKnob.h"

#include <memory>
#include <stdexcept>

// Stage S2: a real player, loaded with four noise stems, feeding deck A's 20
// spatial channels the way MainComponent does in two-deck mode.
namespace
{
constexpr double rate = 44100.0;
constexpr int block = 128;
constexpr int stems = StemDeckPlayer::numStems;
constexpr int bformatFirst = 0, cueFirst = 16, phonesFirst = 18;

int blocksFor (double seconds) { return (int) (seconds * rate) / block + 1; }

void writeNoiseStem (const juce::File& file, int seed)
{
	juce::AudioBuffer<float> audio (2, (int) rate * 3);
	juce::Random random (seed);
	for (int ch = 0; ch < 2; ++ch)
		for (int i = 0; i < audio.getNumSamples(); ++i)
			audio.setSample (ch, i, 0.25f * (2.0f * random.nextFloat() - 1.0f));

	juce::WavAudioFormat wav;
	std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
	auto writer = stream == nullptr ? nullptr
	                                : wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}
	                                                                   .withSampleRate (rate)
	                                                                   .withNumChannels (2)
	                                                                   .withBitsPerSample (24));
	if (writer == nullptr || ! writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples()))
		throw std::runtime_error ("cannot write " + file.getFullPathName().toStdString());
}

class TwoDeckSpatial : public ::testing::Test
{
protected:
	void SetUp() override
	{
		ASSERT_TRUE (folder.createDirectory().wasOk());
		formats.registerBasicFormats();
		StemSet set;
		set.name = "noise";
		for (int s = 0; s < stems; ++s)
		{
			set.files[(size_t) s] = folder.getChildFile ("stem" + juce::String (s) + ".wav");
			writeNoiseStem (set.files[(size_t) s], 17 + s);
		}
		ASSERT_EQ (player.load (set), juce::String());
		ASSERT_EQ (ports.create(), juce::String());
		player.prepareToPlay (block, rate);
		ports.prepare (rate, block);
	}

	void TearDown() override { folder.deleteRecursively(); }

	// One audio block, as MainComponent's getNextAudioBlock runs it for deck A.
	void renderBlock()
	{
		player.getNextAudioBlock (juce::AudioSourceChannelInfo (&deck, 0, block));
		ports.render (player, bus, outputs::spatialFirstChannel (0), block);
	}

	void renderBlocks (int count)
	{
		for (int i = 0; i < count; ++i)
			renderBlock();
	}

	// Channel 0..19 of deck A's spatial ports, in this block.
	const float* spatialChannel (int channel) const { return bus.getReadPointer (outputs::spatialFirstChannel (0) + channel); }
	float spatialPeak (int channel) const { return bus.getMagnitude (outputs::spatialFirstChannel (0) + channel, 0, block); }

	static bool exactlySilent (const float* samples, int count)
	{
		for (int i = 0; i < count; ++i)
			if (samples[i] != 0.0f)
				return false;
		return true;
	}

	void setAllStems (bool muted, bool cued, float gain)
	{
		for (int s = 0; s < stems; ++s)
		{
			player.setStemMuted (s, muted);
			player.setStemCue (s, cued);
			player.setStemGain (s, gain);
		}
	}

	juce::File folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("stemdeck-ports", "");
	juce::AudioFormatManager formats;
	StemDeckPlayer player { formats };
	DeckSpatialPorts ports;
	juce::AudioBuffer<float> deck { StemDeckPlayer::numOutputChannels, block };
	juce::AudioBuffer<float> bus { outputs::channelCount(), block };
};
}

// R4: a stem knob at its bottom is exact silence everywhere a stem goes --
// the pre-mute block, the B-format, the binaural cue and the phones -- even
// with every stem on cue, once the knob's ramp has settled.
TEST_F (TwoDeckSpatial, StemGainMinimumIsExactSilenceOnEverySpatialPort)
{
	setAllStems (false, true, 1.0f);
	player.play();
	renderBlocks (blocksFor (0.3));
	ASSERT_GT (spatialPeak (bformatFirst), 0.0f) << "the deck plays into its B-format";
	ASSERT_GT (spatialPeak (cueFirst), 0.0f) << "and into its cue";

	for (int s = 0; s < stems; ++s)
		player.setStemGain (s, juce::Decibels::decibelsToGain (stemKnobMinimumDb, stemKnobMinimumDb));
	renderBlocks (blocksFor (0.5));

	for (int i = 0; i < blocksFor (0.2); ++i)
	{
		renderBlock();
		ASSERT_EQ (player.getPreMuteBlockSamples(), block);
		for (int ch = 0; ch < StemDeckPlayer::numOutputChannels; ++ch)
			ASSERT_TRUE (exactlySilent (player.getPreMuteBlock().getReadPointer (ch), block)) << "pre-mute channel " << ch;
		for (int ch = 0; ch < outputs::spatialChannelsPerDeck(); ++ch)
			ASSERT_TRUE (exactlySilent (spatialChannel (ch), block)) << outputs::portName (outputs::spatialFirstChannel (0) + ch);
	}
}

// The pre-mute block is the stem after its knob, before its mute: muted, a
// stem still fills it, and with its cue switch settled it is the cue send.
TEST_F (TwoDeckSpatial, ThePreMuteBlockIsTheStemAfterItsKnobBeforeItsMute)
{
	setAllStems (true, true, 0.5f);
	player.play();
	renderBlocks (blocksFor (0.1));

	ASSERT_EQ (player.getPreMuteBlockSamples(), block);
	ASSERT_EQ (player.getCueBlockSamples(), block);
	for (int ch = 0; ch < StemDeckPlayer::numOutputChannels; ++ch)
	{
		EXPECT_GT (player.getPreMuteBlock().getMagnitude (ch, 0, block), 0.0f) << ch;
		EXPECT_EQ (deck.getMagnitude (ch, 0, block), 0.0f) << "muted: the stem output is silent, channel " << ch;
		for (int i = 0; i < block; ++i)
			ASSERT_EQ (player.getPreMuteBlock().getSample (ch, i), player.getCueBlock().getSample (ch, i)) << ch << "/" << i;
	}
}

// MUTE leaves the deck's B-format exactly silent but not its cue (R5), and
// in two-deck mode the phones pair carries the cue (level 1, mix 0).
TEST_F (TwoDeckSpatial, MuteSilencesTheBformatButNotTheCue)
{
	setAllStems (true, true, 1.0f);
	player.play();
	renderBlocks (blocksFor (0.2));

	for (int c = 0; c < 16; ++c)
		EXPECT_TRUE (exactlySilent (spatialChannel (bformatFirst + c), block)) << "bformat " << c;
	for (int side = 0; side < 2; ++side)
	{
		EXPECT_GT (spatialPeak (cueFirst + side), 0.0f) << "cue " << side;
		for (int i = 0; i < block; ++i)
			ASSERT_EQ (spatialChannel (phonesFirst + side)[i], spatialChannel (cueFirst + side)[i]) << side << "/" << i;
	}
}

// Unmuted and off cue: B-format carries the deck, the cue and phones are silent.
TEST_F (TwoDeckSpatial, OffCueTheDeckGoesToTheBformatOnly)
{
	setAllStems (false, false, 1.0f);
	player.play();
	renderBlocks (blocksFor (0.2));

	EXPECT_GT (spatialPeak (bformatFirst), 0.0f);
	for (int side = 0; side < 2; ++side)
	{
		EXPECT_TRUE (exactlySilent (spatialChannel (cueFirst + side), block)) << "cue " << side;
		EXPECT_TRUE (exactlySilent (spatialChannel (phonesFirst + side), block)) << "phones " << side;
	}
}

// A deck that stands gives no pre-mute block and exactly silent spatial ports.
TEST_F (TwoDeckSpatial, AStandingDeckIsExactSilence)
{
	setAllStems (false, true, 1.0f);
	renderBlocks (blocksFor (0.1));

	EXPECT_EQ (player.getPreMuteBlockSamples(), 0);
	for (int ch = 0; ch < outputs::spatialChannelsPerDeck(); ++ch)
		EXPECT_TRUE (exactlySilent (spatialChannel (ch), block)) << ch;
}

// The ports write deck A's 20 channels and nothing else: today's twelve and
// deck B's stay as they were.
TEST_F (TwoDeckSpatial, WritesOnlyItsOwnTwentyChannels)
{
	setAllStems (false, true, 1.0f);
	player.play();
	renderBlocks (blocksFor (0.1));

	for (int ch = 0; ch < outputs::channelCount(); ++ch)
		juce::FloatVectorOperations::fill (bus.getWritePointer (ch), 0.5f, block);
	renderBlock();

	const auto first = outputs::spatialFirstChannel (0);
	for (int ch = 0; ch < outputs::channelCount(); ++ch)
	{
		if (ch >= first && ch < first + outputs::spatialChannelsPerDeck())
			continue;
		for (int i = 0; i < block; ++i)
			ASSERT_EQ (bus.getSample (ch, i), 0.5f) << outputs::portName (ch);
	}
}

// Two-deck mode: deck 3D 1, phones = cue, StemDeck's own phones decoder off.
TEST_F (TwoDeckSpatial, TwoDeckControlsAreFixed)
{
	auto* spatial = ports.deckSpatial();
	ASSERT_NE (spatial, nullptr);
	auto& c = spatial->controls();
	EXPECT_EQ (c.deckThreeD.load(), 1.0f);
	EXPECT_EQ (c.phonesLevel.load(), 1.0f);
	EXPECT_EQ (c.phonesMix.load(), 0.0f);
	EXPECT_FALSE (c.phonesDecoderOn.load());

	setAllStems (false, false, 1.0f);
	player.setStemMuted (2, true);
	player.setStemCue (1, true);
	player.play();
	renderBlock();
	for (int s = 0; s < stems; ++s)
	{
		EXPECT_EQ (c.muted[(size_t) s].load(), s == 2) << s;
		EXPECT_EQ (c.cued[(size_t) s].load(), s == 1) << s;
	}
}

// R14: the spatial deck stands while the player does, and plays when it plays.
TEST_F (TwoDeckSpatial, TheSpatialDeckFollowsThePlayersTransport)
{
	auto& c = ports.deckSpatial()->controls();
	setAllStems (false, true, 1.0f);
	renderBlock();
	EXPECT_FALSE (c.playing.load());

	player.play();
	renderBlock();
	EXPECT_TRUE (c.playing.load());

	player.stop();
	renderBlocks (blocksFor (0.1));
	ports.deckSpatial()->resetTiming();
	renderBlocks (blocksFor (0.1));
	EXPECT_FALSE (c.playing.load());
	EXPECT_EQ (ports.deckSpatial()->timing().encoders, 0.0) << "a stopped deck stands";
}
