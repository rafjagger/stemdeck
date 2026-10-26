// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "OneDeckAudio.h"
#include "Outputs.h"
#include "StemKnob.h"
#include "TwoDeckFixture.h"

// A -bformat instance's audio (S4.4a): one DeckHost on the 22 one-deck
// channels, through the AudioSource the JACK client pulls.
namespace
{
using twodeck::rate;
using twodeck::block;
using twodeck::blocksFor;
constexpr int stems = StemDeckPlayer::numStems;

class OneDeck : public ::testing::Test
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
			twodeck::writeNoiseStem (set.files[(size_t) s], 31 + s);
		}
		ASSERT_EQ (audio.createSpatial(), juce::String());
		audio.prepareToPlay (block, rate);
		ASSERT_EQ (audio.host().load (set), juce::String());
		player().setLoop (0.5, 2.5);   // the noise is 3 s long; the tests play longer
		for (int s = 0; s < stems; ++s)
			settings().threeD[(size_t) s] = 0.5f;   // a share on the encoders, a share in the bed
	}

	void TearDown() override
	{
		pool.removeAllJobs (true, 5000);
		audio.releaseResources();
	}

	~OneDeck() override { folder.deleteRecursively(); }

	StemDeckPlayer& player() { return audio.host().player(); }
	DeckSpatial::Controls& settings() { return audio.host().spatialPorts().settings(); }

	void renderBlock()
	{
		out.clear();
		audio.getNextAudioBlock (juce::AudioSourceChannelInfo (&out, 0, block));
	}

	void renderBlocks (int count)
	{
		for (int i = 0; i < count; ++i)
			renderBlock();
	}

	float peak (int channel) const { return out.getMagnitude (channel, 0, block); }

	// The highest peak of `channel` over the next `seconds`.
	float peakOver (int channel, double seconds)
	{
		float highest = 0.0f;
		for (int i = 0; i < blocksFor (seconds); ++i)
		{
			renderBlock();
			highest = juce::jmax (highest, peak (channel));
		}
		return highest;
	}

	// Every sample of channels [first, first + count) is 0.0f, block after block.
	void expectExactlySilentFor (int first, int count, double seconds, const char* what)
	{
		for (int i = 0; i < blocksFor (seconds); ++i)
		{
			renderBlock();
			for (int ch = first; ch < first + count; ++ch)
				for (int n = 0; n < block; ++n)
					ASSERT_EQ (out.getSample (ch, n), 0.0f) << what << ": " << outputs::oneDeck::portName (ch) << " sample " << n;
		}
	}

	void setAllStems (bool muted, bool cued, float gain)
	{
		for (int s = 0; s < stems; ++s)
		{
			player().setStemMuted (s, muted);
			player().setStemCue (s, cued);
			player().setStemGain (s, gain);
		}
	}

	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	juce::File folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("stemdeck-onedeck", "");
	juce::AudioFormatManager formats;
	juce::ThreadPool pool;
	AnalysisCache cache { folder.getChildFile ("analysis.xml") };
	OneDeckAudio audio { formats, pool, cache };
	juce::AudioBuffer<float> out { outputs::oneDeck::channelCount(), block };
	const outputs::SpatialLayout layout = outputs::oneDeck::layout();
};
}

// R4 through the instance's own path: the GAIN knob at its bottom, MUTE,
// phones LEVEL 0 and either end of CUE<->MIX each leave their outputs
// exactly 0.0f once the ramps have settled.
TEST_F (OneDeck, VolumeMinimaAreExactSilence)
{
	audio.updatePhonesDecoder (spatial::PhonesMode::on, true, false);
	audio.setPhones (1.0f, 0.5f);
	setAllStems (false, true, 1.0f);
	player().play();
	renderBlocks (blocksFor (0.3));
	for (const auto ch : { layout.bformat, layout.cue, layout.aux, layout.phones })
		ASSERT_GT (peakOver (ch, 0.05), 0.0f) << "the deck plays: " << outputs::oneDeck::portName (ch);

	// GAIN at the knob's minimum: every one of the 22.
	setAllStems (false, true, juce::Decibels::decibelsToGain (stemKnobMinimumDb, stemKnobMinimumDb));
	renderBlocks (blocksFor (0.5));
	expectExactlySilentFor (0, layout.channels, 0.2, "GAIN at its minimum");

	// MUTE: the B-format and the AUX; the cue is pre-mute and plays on.
	setAllStems (true, true, 1.0f);
	renderBlocks (blocksFor (0.5));
	EXPECT_GT (peakOver (layout.cue, 0.05), 0.0f) << "the cue is pre-mute";
	expectExactlySilentFor (layout.bformat, outputs::bformatChannels, 0.2, "MUTE");
	expectExactlySilentFor (layout.aux, 2, 0.2, "MUTE");

	// CUE<->MIX at MIX: the cue's weight is exactly 0, and the muted deck's mix is silence.
	audio.setPhones (1.0f, 1.0f);
	renderBlocks (blocksFor (0.5));
	ASSERT_GT (peakOver (layout.cue, 0.05), 0.0f);
	expectExactlySilentFor (layout.phones, 2, 0.2, "CUE<->MIX at MIX");

	// CUE<->MIX at CUE with nothing cued: the deck plays, the phones are silent.
	setAllStems (false, false, 1.0f);
	audio.setPhones (1.0f, 0.0f);
	renderBlocks (blocksFor (0.5));
	ASSERT_GT (peakOver (layout.bformat, 0.05), 0.0f);
	expectExactlySilentFor (layout.phones, 2, 0.2, "CUE<->MIX at CUE, nothing cued");

	// LEVEL 0: the phones, whatever else plays.
	setAllStems (false, true, 1.0f);
	audio.setPhones (0.0f, 0.5f);
	renderBlocks (blocksFor (0.5));
	ASSERT_GT (peakOver (layout.cue, 0.05), 0.0f);
	expectExactlySilentFor (layout.phones, 2, 0.2, "phones LEVEL 0");
}

// R12: StemDeck's own phones decoder runs only with its ports connected, in
// automatic only while Core is not heard; `on` ignores Core, `off` never runs.
// With nothing cued and the phones at MIX, the phones carry sound exactly
// when it runs.
TEST_F (OneDeck, PhonesDecoderFollowsR12)
{
	using spatial::PhonesMode;
	setAllStems (false, false, 1.0f);
	audio.setPhones (1.0f, 1.0f);
	player().play();

	for (const auto mode : { PhonesMode::automatic, PhonesMode::on, PhonesMode::off })
		for (const auto connected : { false, true })
			for (const auto coreHeard : { false, true })
			{
				const auto expected = spatial::phonesDecoderRuns (mode, connected, coreHeard);
				audio.updatePhonesDecoder (mode, connected, coreHeard);
				EXPECT_EQ (audio.phonesDecoderOn(), expected);
				renderBlocks (blocksFor (0.3));   // ramps, hold-over and the IR tail run out
				const auto runsBefore = audio.host().spatialPorts().deckSpatial()->decoderRuns().phones;
				const auto heard = peakOver (layout.phones, 0.1);
				const auto ran = audio.host().spatialPorts().deckSpatial()->decoderRuns().phones > runsBefore;
				EXPECT_EQ (heard > 0.0f, expected) << "mode " << (int) mode << ", connected " << connected << ", Core heard " << coreHeard;
				EXPECT_EQ (ran, expected) << "mode " << (int) mode << ", connected " << connected << ", Core heard " << coreHeard;
			}
}

// Core is heard while a /core/here came within the last five seconds (Core
// announces every two).
TEST (OneDeckCore, HeardWithinFiveSeconds)
{
	EXPECT_FALSE (spatial::coreHeard (std::nullopt, 100.0));
	EXPECT_TRUE (spatial::coreHeard (100.0, 100.0));
	EXPECT_TRUE (spatial::coreHeard (100.0, 104.9));
	EXPECT_FALSE (spatial::coreHeard (100.0, 105.1));
}
