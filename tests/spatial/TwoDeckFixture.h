// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <gtest/gtest.h>

#include "DeckSpatialPorts.h"
#include "Outputs.h"
#include "StemDeckPlayer.h"

#include <memory>
#include <stdexcept>

#include <cmath>

// Stage S2: a real player, loaded with four noise stems, feeding deck A's 20
// spatial channels the way MainComponent does in two-deck mode. Shared by
// test files, so a named namespace: gtest needs one fixture type per suite.
namespace twodeck
{
constexpr double rate = 44100.0;
constexpr int block = 128;
constexpr int stems = StemDeckPlayer::numStems;
constexpr int bformatFirst = 0, cueFirst = 16, phonesFirst = 18;

inline int blocksFor (double seconds) { return (int) (seconds * rate) / block + 1; }

inline void writeNoiseStem (const juce::File& file, int seed)
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

// Stage S3: the one-shots in the same audio path.
class DeckSpatialMotion : public TwoDeckSpatial
{
protected:
	// The cue's left-minus-right level in dB over the next `seconds`.
	double cueBalanceDb (double seconds)
	{
		double left = 0.0, right = 0.0;
		for (int i = 0; i < blocksFor (seconds); ++i)
		{
			renderBlock();
			for (int n = 0; n < block; ++n)
			{
				left += (double) spatialChannel (cueFirst)[n] * spatialChannel (cueFirst)[n];
				right += (double) spatialChannel (cueFirst + 1)[n] * spatialChannel (cueFirst + 1)[n];
			}
		}
		return 10.0 * std::log10 ((left + 1e-30) / (right + 1e-30));
	}
};
}
