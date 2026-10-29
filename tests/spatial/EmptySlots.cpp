// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "OneDeckAudio.h"
#include "Outputs.h"
#include "SectionAnalysis.h"
#include "StemDeckPlayer.h"
#include "StemLevels.h"
#include "StemSet.h"
#include "TempoAnalysis.h"
#include "TwoDeckFixture.h"

#include <chrono>
#include <cmath>
#include <thread>

// A set of 1-4 stereo files (2026-10-29): file n on slot n, the other slots
// empty and silent -- found in the library, played, handed over to by
// CONTINUE and analysed, with nothing on an empty slot but exact zeros.
namespace
{
using twodeck::rate;
using twodeck::block;
using twodeck::blocksFor;
constexpr int stems = StemSet::numStems;
constexpr double flacSeconds = 1.5;

void writeFlacNoise (const juce::File& file, int seed, double seconds = flacSeconds)
{
	const auto length = (int) std::lround (seconds * rate);
	juce::AudioBuffer<float> audio (2, length);
	juce::Random random (seed);
	for (int ch = 0; ch < 2; ++ch)
		for (int i = 0; i < length; ++i)
			audio.setSample (ch, i, 0.25f * (2.0f * random.nextFloat() - 1.0f));

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

class EmptySlots : public ::testing::Test
{
protected:
	void SetUp() override
	{
		ASSERT_TRUE (folder.createDirectory().wasOk());
		formats.registerBasicFormats();
	}

	~EmptySlots() override { folder.deleteRecursively(); }

	// A set with files on `slots` only: WAV (mapped) or FLAC (decoded).
	StemSet makeSet (const juce::String& name, std::initializer_list<int> slots, bool flac)
	{
		StemSet set;
		set.name = name;
		for (const auto s : slots)
		{
			const auto file = folder.getChildFile (name + " - " + juce::String (s + 1) + " - part" + (flac ? ".flac" : ".wav"));
			if (flac)
				writeFlacNoise (file, 11 + s);
			else
				twodeck::writeNoiseStem (file, 11 + s);
			set.files[(size_t) s] = file;
			set.stemNames[(size_t) s] = juce::String (s + 1) + " - part";
		}
		set.lengthSeconds = flac ? flacSeconds : 3.0;
		return set;
	}

	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	juce::File folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("stemdeck-empty-slots", "");
	juce::AudioFormatManager formats;
};

// Every sample of `buffer` is a finite number.
void expectFinite (const juce::AudioBuffer<float>& buffer, const char* what)
{
	for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
		for (int n = 0; n < buffer.getNumSamples(); ++n)
			ASSERT_TRUE (std::isfinite (buffer.getSample (ch, n))) << what << ": channel " << ch << " sample " << n;
}
}

//==============================================================================
TEST_F (EmptySlots, TheLibraryFindsSetsOfOneToFourFiles)
{
	for (const auto* name : { "T - 1 - drums.wav", "T - 3 - other.wav", "Lone.wav", "01 - Intro.wav", "02 - Song.wav",
							  "F - 1 - a.wav", "F - 2 - b.wav", "F - 3 - c.wav", "F - 4 - d.wav" })
		twodeck::writeNoiseStem (folder.getChildFile (name), 1);
	ASSERT_TRUE (folder.getChildFile ("originals").createDirectory().wasOk());
	twodeck::writeNoiseStem (folder.getChildFile ("originals/Lone original.wav"), 1);

	const auto sets = StemSet::scanFolder (folder, formats);
	std::map<juce::String, const StemSet*> byName;
	for (const auto& set : sets)
		byName[set.name] = &set;
	ASSERT_EQ (byName.size(), 5u) << "T, Lone, 01 - Intro, 02 - Song, F; nothing from originals/";

	const auto& two = *byName.at ("T");
	EXPECT_EQ (two.files[0].getFileName(), "T - 1 - drums.wav");
	EXPECT_EQ (two.files[2].getFileName(), "T - 3 - other.wav");
	EXPECT_FALSE (two.hasStem (1));
	EXPECT_FALSE (two.hasStem (3));
	EXPECT_EQ (two.numLoaded(), 2);
	EXPECT_FALSE (two.isComplete());
	EXPECT_NEAR (two.lengthSeconds, 3.0, 0.01);

	const auto& lone = *byName.at ("Lone");
	EXPECT_EQ (lone.files[0].getFileName(), "Lone.wav");
	EXPECT_EQ (lone.numLoaded(), 1);
	EXPECT_EQ (lone.firstFile(), lone.files[0]);
	EXPECT_EQ (byName.at ("01 - Intro")->numLoaded(), 1) << "an album's tracks are sets of one each";

	EXPECT_TRUE (byName.at ("F")->isComplete());

	StemSet secondOnly;
	secondOnly.files[1] = folder.getChildFile ("x.wav");
	EXPECT_EQ (secondOnly.firstFile(), secondOnly.files[1]) << "the id of a set without slot 1";
}

// The player alone: the empty slots' output pairs are exactly 0.0f and their
// levels 0, the loaded ones play -- mapped (WAV) and decoded (FLAC).
TEST_F (EmptySlots, ThePlayerPlaysTheLoadedSlotsAndSilenceOnTheRest)
{
	for (const auto flac : { false, true })
	{
		for (const auto& slots : { std::vector<int> { 0 }, std::vector<int> { 1, 3 } })
		{
			StemSet set;
			if (slots.size() == 1)
				set = makeSet (flac ? "lone-flac" : "lone-wav", { 0 }, flac);
			else
				set = makeSet (flac ? "two-flac" : "two-wav", { 1, 3 }, flac);
			const auto label = set.name.toStdString();

			StemDeckPlayer player { formats };
			player.prepareToPlay (block, rate);
			ASSERT_EQ (player.load (set), juce::String()) << label;
			ASSERT_TRUE (waitFor ([&player] { return player.isReady(); })) << label;
			EXPECT_NEAR (player.getLength(), set.lengthSeconds, 0.01) << label;
			for (int s = 0; s < stems; ++s)
				player.popStemPeak (s);

			player.play();
			juce::AudioBuffer<float> out (StemDeckPlayer::numOutputChannels, block);
			std::array<float, stems> loudest {};
			for (int i = 0; i < blocksFor (0.5); ++i)
			{
				out.clear();
				player.getNextAudioBlock (juce::AudioSourceChannelInfo (&out, 0, block));
				expectFinite (out, label.c_str());
				for (int s = 0; s < stems; ++s)
					loudest[(size_t) s] = juce::jmax (loudest[(size_t) s], out.getMagnitude (2 * s, 0, block), out.getMagnitude (2 * s + 1, 0, block));
			}

			for (int s = 0; s < stems; ++s)
			{
				const auto loaded = set.hasStem (s);
				if (loaded)
				{
					EXPECT_GT (loudest[(size_t) s], 0.01f) << label << " slot " << s + 1 << " plays";
					EXPECT_GT (player.popStemPeak (s), 0.0f) << label << " slot " << s + 1;
				}
				else
				{
					EXPECT_EQ (loudest[(size_t) s], 0.0f) << label << " slot " << s + 1 << " is silent";
					EXPECT_EQ (player.popStemPeak (s), 0.0f) << label << " slot " << s + 1 << ": the desk reads 0";
				}
			}
		}
	}
}

TEST_F (EmptySlots, ASetWithoutAnyFileIsRefused)
{
	StemDeckPlayer player { formats };
	EXPECT_NE (player.load (StemSet {}), juce::String());
}

// The whole one-deck path: nothing but finite numbers on the 43 outputs, and
// with the one loaded slot muted the B-format and the AUX are exactly 0.0f --
// the empty slots' encoders add nothing, not even noise.
TEST_F (EmptySlots, TheEncodersOfEmptySlotsPutOutExactZeros)
{
	juce::ThreadPool pool;
	AnalysisCache cache { folder.getChildFile ("analysis.xml") };
	OneDeckAudio audio { formats, pool, cache };
	const auto layout = outputs::oneDeck::layout();
	juce::AudioBuffer<float> out (outputs::oneDeck::channelCount(), block);
	const auto render = [&] { out.clear(); audio.getNextAudioBlock (juce::AudioSourceChannelInfo (&out, 0, block)); };

	ASSERT_EQ (audio.createSpatial(), juce::String());
	audio.prepareToPlay (block, rate);
	const auto lone = makeSet ("lone", { 0 }, false);
	ASSERT_EQ (audio.host().load (lone), juce::String());
	auto& player = audio.host().player();
	ASSERT_TRUE (waitFor ([&player] { return player.isReady(); }));
	auto& settings = audio.host().spatialPorts().settings();
	for (int s = 0; s < stems; ++s)
	{
		settings.aux[(size_t) s] = 1.0f;
	}
	player.setLoop (0.5, 2.5);
	player.play();

	float bformat = 0.0f;
	for (int i = 0; i < blocksFor (0.5); ++i)
	{
		render();
		expectFinite (out, "playing");
		bformat = juce::jmax (bformat, out.getMagnitude (layout.bformat, 0, block));
	}
	EXPECT_GT (bformat, 0.0f) << "slot 1 plays";

	player.setStemMuted (0, true);
	for (int i = 0; i < blocksFor (0.5); ++i)
		render();
	for (int i = 0; i < blocksFor (0.2); ++i)
	{
		render();
		for (int ch = layout.bformat; ch < layout.bformat + 2 * outputs::bformatChannels; ++ch)
			for (int n = 0; n < block; ++n)
				ASSERT_EQ (out.getSample (ch, n), 0.0f) << outputs::oneDeck::portName (ch) << " sample " << n;
	}
	pool.removeAllJobs (true, 5000);
	audio.releaseResources();
}

// CONTINUE from a full set into a set of one: the change happens, and after
// it slots 2-4 are silent.
TEST_F (EmptySlots, ContinueHandsOverToASetOfOne)
{
	const auto full = makeSet ("full", { 0, 1, 2, 3 }, true);
	const auto lone = makeSet ("next", { 0 }, true);
	StemDeckPlayer player { formats };
	player.prepareToPlay (block, rate);
	ASSERT_EQ (player.load (full), juce::String());
	ASSERT_TRUE (waitFor ([&player] { return player.isReady(); }));
	player.setContinue (true);
	ASSERT_EQ (player.stageNext (lone), juce::String());
	ASSERT_TRUE (waitFor ([&player] { return player.isStagedReady(); }));

	player.play();
	juce::AudioBuffer<float> out (StemDeckPlayer::numOutputChannels, block);
	for (int i = 0; i < blocksFor (flacSeconds + 0.1); ++i)
		player.getNextAudioBlock (juce::AudioSourceChannelInfo (&out, 0, block));
	ASSERT_EQ (player.handovers(), 1);

	float slotOne = 0.0f;
	for (int i = 0; i < blocksFor (0.3); ++i)
	{
		out.clear();
		player.getNextAudioBlock (juce::AudioSourceChannelInfo (&out, 0, block));
		expectFinite (out, "after the change");
		slotOne = juce::jmax (slotOne, out.getMagnitude (0, 0, block));
		for (int ch = 2; ch < StemDeckPlayer::numOutputChannels; ++ch)
			ASSERT_EQ (out.getMagnitude (ch, 0, block), 0.0f) << "channel " << ch;
	}
	EXPECT_GT (slotOne, 0.01f);
}

// The analyses read a set short of four as one mix on slot 1; the levels
// AUTO DJ hands over by are zero on the empty slots, as long as the set.
TEST_F (EmptySlots, AnalysisAndLevelsReadASetShortOfFour)
{
	const auto two = makeSet ("two", { 1, 3 }, false);
	const auto never = [] { return false; };

	const auto read = TempoAnalysis::readStems (two, formats, never);
	ASSERT_TRUE (read.has_value());
	ASSERT_FALSE (read->mono[0].empty()) << "the mix on slot 1";
	EXPECT_GT (*std::max_element (read->mono[0].begin(), read->mono[0].end()), 0.0f);
	for (int s = 1; s < stems; ++s)
		for (const auto sample : read->mono[(size_t) s])
			ASSERT_EQ (sample, 0.0f) << "slot " << s + 1;

	const auto features = SectionAnalysis::analyse (two, formats, { 120.0, 0.0 }, never);
	ASSERT_TRUE (features.has_value());
	ASSERT_FALSE (features->frames.empty());
	EXPECT_GT (features->frames[1].rms[0], 0.0f);
	EXPECT_EQ (features->frames[1].rms[1], 0.0f);

	const auto levels = StemLevels::read (two, formats, never);
	ASSERT_NE (levels, nullptr);
	ASSERT_FALSE ((*levels)[1].rms.empty());
	EXPECT_EQ ((*levels)[0].rms.size(), (*levels)[1].rms.size());
	for (const auto value : (*levels)[0].rms)
		ASSERT_EQ (value, 0.0f);
	EXPECT_GT (*std::max_element ((*levels)[3].rms.begin(), (*levels)[3].rms.end()), 0.0f);
}
