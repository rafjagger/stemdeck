// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "OneDeckAudio.h"
#include "OneDeckView.h"
#include "Outputs.h"
#include "StemKnob.h"
#include "Theme.h"
#include "TwoDeckFixture.h"

#include <vector>

// A -bformat instance's audio (S4.4a): one DeckHost on the 43 one-deck
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
		set.name = "noise";
		const std::array<const char*, stems> names { "drums", "bass", "vocals", "synth" };
		for (int s = 0; s < stems; ++s)
		{
			set.files[(size_t) s] = folder.getChildFile ("stem" + juce::String (s) + ".wav");
			set.stemNames[(size_t) s] = names[(size_t) s];
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

	void setAllAux (float aux)
	{
		for (auto& send : settings().aux)
			send.store (aux);
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
	StemSet set;
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
	setAllAux (1.0f);
	renderBlocks (blocksFor (0.3));
	for (const auto ch : { layout.bformat, layout.aux, layout.cue, layout.phones })
		ASSERT_GT (peakOver (ch, 0.05), 0.0f) << "the deck plays: " << outputs::oneDeck::portName (ch);

	// GAIN at the knob's minimum: every one of the 43.
	setAllStems (false, true, juce::Decibels::decibelsToGain (stemKnobMinimumDb, stemKnobMinimumDb));
	renderBlocks (blocksFor (0.5));
	expectExactlySilentFor (0, layout.channels, 0.2, "GAIN at its minimum");

	// MUTE: the B-format and the AUX; the cue is pre-mute and plays on, in its
	// own 9 channels and in the phones.
	setAllStems (true, true, 1.0f);
	renderBlocks (blocksFor (0.5));
	EXPECT_GT (peakOver (layout.cue, 0.05), 0.0f) << "the cue is pre-mute";
	EXPECT_GT (peakOver (layout.phones, 0.05), 0.0f) << "the cue is pre-mute";
	expectExactlySilentFor (layout.bformat, outputs::bformatChannels, 0.2, "MUTE");
	expectExactlySilentFor (layout.aux, outputs::bformatChannels, 0.2, "MUTE");

	// CUE<->MIX at MIX: the cue's weight is exactly 0, and the muted deck's mix is silence.
	audio.setPhones (1.0f, 1.0f);
	renderBlocks (blocksFor (0.5));
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
	ASSERT_GT (peakOver (layout.bformat, 0.05), 0.0f);
	expectExactlySilentFor (layout.phones, 2, 0.2, "LEVEL 0");

	// AUX 0: the send, settled, leaves the aux bus exactly 0.0f.
	setAllStems (false, true, 1.0f);
	setAllAux (0.0f);
	renderBlocks (blocksFor (0.5));
	expectExactlySilentFor (layout.aux, outputs::bformatChannels, 0.2, "AUX 0");
	ASSERT_GT (peakOver (layout.bformat, 0.05), 0.0f);
	expectExactlySilentFor (layout.phones, 2, 0.2, "phones LEVEL 0");
}

// R12: StemDeck's own phones decoder runs only with its ports connected, in
// automatic only while Core is not heard; `on` ignores Core, `off` never runs.
// With nothing cued and the phones at MIX, the phones carry sound exactly
// when it runs.
// PipeWire calls the sample-rate callback before a block size is known: the
// whole deck (player and engine) refuses a block of 0 and prepares at 128 later.
TEST_F (OneDeck, PrepareWithBlockSizeZeroIsRefused)
{
	EXPECT_FALSE (audio.host().prepare (rate, 0));
	audio.prepareToPlay (0, rate);   // the JACK path: must not divide by zero
	EXPECT_TRUE (audio.host().prepare (rate, block));
	setAllStems (false, false, 1.0f);
	player().play();
	renderBlocks (blocksFor (0.3));
	EXPECT_GT (peakOver (layout.bformat, 0.05), 0.0f);
}

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

//==============================================================================
// The v4 screen (S4.4b): the deck part of a -bformat instance, on the
// fixture's deck. Components need JUCE's GUI; the binary runs under xvfb.
namespace
{
constexpr int screenWidth = 768, viewHeight = 760;   // the rig's width; the deck part of its 1024

// How much a snapshot varies: a screen that painted only its ground is flat.
double lumaVariance (const juce::Image& image)
{
	const juce::Image::BitmapData pixels (image, juce::Image::BitmapData::readOnly);
	double sum = 0.0, squares = 0.0;
	const auto count = (double) image.getWidth() * image.getHeight();
	for (int y = 0; y < image.getHeight(); ++y)
		for (int x = 0; x < image.getWidth(); ++x)
		{
			const auto colour = pixels.getPixelColour (x, y);
			const double luma = (colour.getRed() + colour.getGreen() + colour.getBlue()) / 3.0;
			sum += luma;
			squares += luma * luma;
		}
	const auto mean = sum / count;
	return squares / count - mean * mean;
}

int differingPixels (const juce::Image& a, const juce::Image& b)
{
	const juce::Image::BitmapData first (a, juce::Image::BitmapData::readOnly), second (b, juce::Image::BitmapData::readOnly);
	int differing = 0;
	for (int y = 0; y < a.getHeight(); ++y)
		for (int x = 0; x < a.getWidth(); ++x)
			differing += first.getPixelColour (x, y) != second.getPixelColour (x, y) ? 1 : 0;
	return differing;
}

juce::Image snapshotOf (OneDeckView& view)
{
	view.refresh();
	return view.createComponentSnapshot (view.getLocalBounds());
}
}

// Every state the screen shows, under the app's look and feel: each snapshot
// painted more than a ground, and each state changed the picture.
TEST_F (OneDeck, PaintsEveryState)
{
	DJLookAndFeel look;
	constexpr double minimumVariance = 50.0;
	{
		OneDeckAudio empty { formats, pool, cache };
		OneDeckView view (empty, formats, 0);
		view.setLookAndFeel (&look);
		view.setSize (screenWidth, viewHeight);
		const auto image = snapshotOf (view);
		EXPECT_GT (lumaVariance (image), minimumVariance) << "empty";
		view.setLookAndFeel (nullptr);
	}

	OneDeckView view (audio, formats, 0);
	view.setLookAndFeel (&look);
	view.setSize (screenWidth, viewHeight);
	std::vector<std::pair<std::string, juce::Image>> shots;
	const auto shoot = [&] (const char* state) { shots.emplace_back (state, snapshotOf (view)); };

	view.setSet (set);
	shoot ("loaded");

	player().setBeatGrid ({ 120.0, 0.0 });   // bar lines every 2 s; the deck stands at 0.5
	view.column (1).oneShots().pad (motionlite::Shape::ping).onClick();
	renderBlock();
	ASSERT_EQ (audio.host().spatialPorts().motion().stemState (1), motionlite::PadState::armed);
	shoot ("stem armed");

	view.shift().setToggleState (true, juce::dontSendNotification);
	view.column (2).oneShots().pad (motionlite::Shape::orbit).onClick();
	renderBlock();
	ASSERT_EQ (audio.host().spatialPorts().motion().stemState (2), motionlite::PadState::running);
	EXPECT_FALSE (view.shift().getToggleState()) << "SHIFT holds for one press";
	shoot ("running");

	view.column (2).cue().setToggleState (true, juce::sendNotificationSync);
	ASSERT_TRUE (player().isStemCued (2));
	shoot ("cued");

	view.column (3).mute().setToggleState (true, juce::sendNotificationSync);
	ASSERT_TRUE (player().isStemMuted (3));
	shoot ("muted");

	view.outputs().phonesMix().setValue (1.0, juce::sendNotificationSync);
	ASSERT_EQ (settings().phonesMix.load(), 1.0f);
	shoot ("phones mix");

	for (size_t i = 0; i < shots.size(); ++i)
	{
		const auto& [state, image] = shots[i];
		EXPECT_EQ (image.getWidth(), screenWidth) << state;
		EXPECT_GT (lumaVariance (image), minimumVariance) << state;
		if (i > 0)
			EXPECT_GT (differingPixels (shots[i - 1].second, image), 0) << state << " looks like " << shots[i - 1].first;
	}
	view.setLookAndFeel (nullptr);
}

// GAIN is the player's stem gain (its bottom exact silence, R4), 3D, WIDTH
// and the viewer's home the spatial settings -- never the played pose, which
// is the audio thread's -- and CUE and MUTE the player's switches. Only the
// column's own stem.
TEST_F (OneDeck, StemColumnKnobsDriveTheControls)
{
	OneDeckView view (audio, formats, 0);
	view.setSize (screenWidth, viewHeight);
	auto& column = view.column (1);
	const auto& played = audio.host().spatialPorts().deckSpatial()->controls();
	const auto playedThreeD = played.threeD[1].load();

	column.gain().setValue (-6.0, juce::sendNotificationSync);
	EXPECT_FLOAT_EQ (player().getStemGain (1), juce::Decibels::decibelsToGain (-6.0f));
	column.gain().setValue (column.gain().getMinimum(), juce::sendNotificationSync);
	EXPECT_EQ (player().getStemGain (1), 0.0f);
	EXPECT_EQ (column.gain().getMinimum(), (double) stemKnobMinimumDb);

	column.threeD().setValue (0.8, juce::sendNotificationSync);
	column.width().setValue (150.0, juce::sendNotificationSync);
	EXPECT_FLOAT_EQ (settings().threeD[1].load(), 0.8f);
	EXPECT_FLOAT_EQ (settings().width[1].load(), 150.0f);
	EXPECT_EQ (played.threeD[1].load(), playedThreeD) << "the played pose is the audio thread's";

	ASSERT_TRUE (column.viewer().onHomeChanged != nullptr);
	column.viewer().onHomeChanged ({ 30.0f, 10.0f, 90.0f });
	EXPECT_FLOAT_EQ (settings().azimuth[1].load(), 30.0f);
	EXPECT_FLOAT_EQ (settings().elevation[1].load(), 10.0f);
	EXPECT_FLOAT_EQ (settings().width[1].load(), 90.0f);
	EXPECT_DOUBLE_EQ (column.width().getValue(), 90.0) << "the WIDTH knob follows the viewer";

	column.cue().setToggleState (true, juce::sendNotificationSync);
	column.mute().setToggleState (true, juce::sendNotificationSync);
	EXPECT_TRUE (player().isStemCued (1));
	EXPECT_TRUE (player().isStemMuted (1));

	for (int other : { 0, 2, 3 })
	{
		EXPECT_FLOAT_EQ (settings().threeD[(size_t) other].load(), 0.5f);
		EXPECT_FLOAT_EQ (settings().width[(size_t) other].load(), 60.0f);
		EXPECT_FLOAT_EQ (player().getStemGain (other), 1.0f);
		EXPECT_FALSE (player().isStemCued (other));
		EXPECT_FALSE (player().isStemMuted (other));
	}
}

// The deck panel's pitch fader is the player's speed, and a restored session
// plays at its stored tempo (the S4.4a placeholder played at 1) with its
// stems' GAIN, MUTE and CUE; a save gives them back.
TEST_F (OneDeck, PitchDrivesTheSpeedAndTheSessionTempoIsRestored)
{
	OneDeckView view (audio, formats, 0);
	view.setSize (screenWidth, viewHeight);
	view.setSet (set);

	view.deckPanel().moveTempo (16.0);   // an eighth of the travel: +2 % at +-8 %
	EXPECT_NEAR (player().getSpeed(), 1.02, 1e-9);

	DeckSession session;
	session.tempo = 1.05;
	session.tempoRange = 0.08;
	session.stems[2].gainDb = -12.0;
	session.stems[2].muted = true;
	session.stems[3].cue = true;
	view.restoreState (session);
	EXPECT_NEAR (player().getSpeed(), 1.05, 1e-9);
	EXPECT_FLOAT_EQ (player().getStemGain (2), juce::Decibels::decibelsToGain (-12.0f));
	EXPECT_TRUE (player().isStemMuted (2));
	EXPECT_TRUE (player().isStemCued (3));

	DeckSession saved;
	view.saveState (saved);
	EXPECT_NEAR (saved.tempo, 1.05, 1e-9);
	EXPECT_NEAR (saved.stems[2].gainDb, -12.0, 1e-9);
	EXPECT_TRUE (saved.stems[2].muted);
	EXPECT_TRUE (saved.stems[3].cue);
	EXPECT_FALSE (saved.stems[0].cue);
}
