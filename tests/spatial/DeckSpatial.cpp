// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "DeckSpatialFixture.h"

#include <iostream>

using namespace sdtest;

namespace
{
constexpr int rampSamples = 441;   // 10 ms at 44.1 kHz

void expectBformatNear (const Rendered& out, const std::array<Signal, spatial::bformatChannels>& reference, float tolerance, int from = settled)
{
	for (int c = 0; c < spatial::bformatChannels; ++c)
		EXPECT_LE (maxDifference (out.bformat[(size_t) c], reference[(size_t) c], from), tolerance) << "channel " << c;
}

void expectAllNear (const Rendered& a, const Rendered& b, float tolerance, const std::string& what)
{
	for (int c = 0; c < spatial::bformatChannels; ++c)
	{
		EXPECT_LE (maxDifference (a.bformat[(size_t) c], b.bformat[(size_t) c]), tolerance) << what << ", bformat " << c;
		EXPECT_LE (maxDifference (a.aux[(size_t) c], b.aux[(size_t) c]), tolerance) << what << ", aux " << c;
	}
	for (size_t c = 0; c < a.cue.size(); ++c)
		EXPECT_LE (maxDifference (a.cue[c], b.cue[c]), tolerance) << what << ", cue " << c;
	for (int side = 0; side < 2; ++side)
		EXPECT_LE (maxDifference (a.phones[(size_t) side], b.phones[(size_t) side]), tolerance) << what << ", phones " << side;
}

void expectEverythingSilent (const Rendered& out, int from, const std::string& what)
{
	for (int c = 0; c < spatial::bformatChannels; ++c)
	{
		EXPECT_TRUE (exactlySilent (out.bformat[(size_t) c], from)) << what << ", bformat " << c;
		EXPECT_TRUE (exactlySilent (out.aux[(size_t) c], from)) << what << ", aux " << c;
	}
	for (size_t c = 0; c < out.cue.size(); ++c)
		EXPECT_TRUE (exactlySilent (out.cue[c], from)) << what << ", cue " << c;
	for (int side = 0; side < 2; ++side)
		EXPECT_TRUE (exactlySilent (out.phones[(size_t) side], from)) << what << ", phones " << side;
}

StemInputs noiseOnStem (int stem, int seed = 1)
{
	StemInputs in (oneSecond);
	in.playNoise (stem, seed);
	return in;
}

StemInputs noiseOnAllStems()
{
	StemInputs in (oneSecond);
	for (int stem = 0; stem < DeckSpatial::stems; ++stem)
		in.playNoise (stem, 10 * (stem + 1));
	return in;
}

// Every path at work: 3D spread over the stems, poses off home, two stems cued, phones mixing.
void busyDeck (DeckSpatial::Controls& c)
{
	const float threeD[] = { 0.0f, 0.3f, 0.7f, 1.0f };
	for (int s = 0; s < DeckSpatial::stems; ++s)
	{
		c.threeD[(size_t) s] = threeD[s];
		c.azimuth[(size_t) s] = -120.0f + 70.0f * (float) s;
		c.elevation[(size_t) s] = 10.0f * (float) s;
		c.width[(size_t) s] = 30.0f + 20.0f * (float) s;
	}
	c.cued[1] = true;
	c.cued[2] = true;
	c.phonesDecoderOn = true;
	c.phonesMix = 0.5f;
	c.phonesLevel = 0.8f;
}
}

TEST (DeckSpatial, ThreeDZeroIsTheBedOnly)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnStem (0);
	auto deck = preparedDeck();
	const auto out = render (*deck, in);
	EXPECT_EQ (deck->movingGainNow (0), 0.0f);
	expectBformatNear (out, bedReference (in.left (0), in.right (0)), 1e-6f);
}

TEST (DeckSpatial, DeckThreeDZeroSilencesTheMovingPath)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnStem (0);
	auto deck = preparedDeck (block, [] (DeckSpatial::Controls& c)
	{
		c.threeD[0] = 1.0f;
		c.deckThreeD = 0.0f;
	});
	const auto bed = bedReference (in.left (0), in.right (0));
	expectBformatNear (render (*deck, in), bed, 1e-6f);
	EXPECT_EQ (deck->movingGainNow (0), 0.0f);

	deck->controls().envelope[0] = 1.0f;   // a one-shot at its peak
	expectBformatNear (render (*deck, in), bed, 1e-6f);
	EXPECT_EQ (deck->movingGainNow (0), 0.0f);
}

TEST (DeckSpatial, ThreeDOneIsTheEncoderOnly)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnStem (0);
	auto deck = preparedDeck (block, [] (DeckSpatial::Controls& c)
	{
		c.threeD[0] = 1.0f;
		c.azimuth[0] = 30.0f;
		c.elevation[0] = 10.0f;
		c.width[0] = 90.0f;
	});
	const auto out = render (*deck, in);
	EXPECT_EQ (deck->movingGainNow (0), 1.0f);
	expectBformatNear (out, encoderReference (in.left (0), in.right (0), { 30.0f, 10.0f, 0.0f, 90.0f }), 1e-6f);
}

TEST (DeckSpatial, DeckThreeDMultiplies)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnStem (0);
	auto halfDeck = preparedDeck (block, [] (DeckSpatial::Controls& c) { c.deckThreeD = 0.5f; c.threeD[0] = 1.0f; });
	auto halfStem = preparedDeck (block, [] (DeckSpatial::Controls& c) { c.deckThreeD = 1.0f; c.threeD[0] = 0.5f; });
	const auto a = render (*halfDeck, in), b = render (*halfStem, in);
	EXPECT_GT (halfDeck->movingGainNow (0), 0.0f);
	EXPECT_EQ (halfDeck->movingGainNow (0), halfStem->movingGainNow (0));
	expectBformatNear (a, b.bformat, 1e-6f, 0);
}

TEST (DeckSpatial, MuteSilencesMainButNotCue)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnStem (0);
	auto deck = preparedDeck();
	const auto out = render (*deck, in, { block, true, [&] (int first)
	{
		// muted only after a while, so the ramp down is part of the run
		auto& c = deck->controls();
		c.threeD[0] = 0.5f;
		c.cued[0] = true;
		c.muted[0] = first >= oneSecond / 4;
	} });
	const int afterRamp = oneSecond / 4 + block + rampSamples;
	for (int c = 0; c < spatial::bformatChannels; ++c)
		EXPECT_TRUE (exactlySilent (out.bformat[(size_t) c], afterRamp)) << "bformat " << c;
	EXPECT_GE (rmsDb (out.cue[0], afterRamp), -30.0) << "the cue's W";
}

TEST (DeckSpatial, VolumeMinimaAreExactSilence)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	{
		// every stem at its GAIN knob's minimum: 0.0f into the deck
		auto deck = preparedDeck (block, busyDeck);
		expectEverythingSilent (render (*deck, StemInputs (oneSecond)), 0, "stems at gain 0");
	}
	const auto in = noiseOnAllStems();
	{
		auto deck = preparedDeck (block, [] (DeckSpatial::Controls& c) { busyDeck (c); c.phonesLevel = 0.0f; });
		const auto out = render (*deck, in);
		EXPECT_TRUE (exactlySilent (out.phones[0])) << "phones level 0";
		EXPECT_TRUE (exactlySilent (out.phones[1])) << "phones level 0";
	}
	{
		// mix 1: phones are the mix alone, whether a stem is cued or not
		auto cued = preparedDeck (block, [] (DeckSpatial::Controls& c) { busyDeck (c); c.phonesMix = 1.0f; c.phonesLevel = 1.0f; });
		auto uncued = preparedDeck (block, [] (DeckSpatial::Controls& c)
		{
			busyDeck (c);
			c.phonesMix = 1.0f;
			c.phonesLevel = 1.0f;
			c.cued[1] = c.cued[2] = false;
		});
		const auto a = render (*cued, in), b = render (*uncued, in);
		EXPECT_FALSE (exactlySilent (a.cue[0])) << "a cue to keep out";
		for (int side = 0; side < 2; ++side)
			EXPECT_EQ (maxDifference (a.phones[(size_t) side], b.phones[(size_t) side]), 0.0f) << "mix 1 carries no cue";
	}
	{
		// mix 0 with nothing cued: the phones carry nothing, and decode nothing
		auto deck = preparedDeck (block, [] (DeckSpatial::Controls& c)
		{
			busyDeck (c);
			c.phonesMix = 0.0f;
			c.phonesLevel = 1.0f;
			c.cued[1] = c.cued[2] = false;
		});
		const auto out = render (*deck, in);
		EXPECT_TRUE (exactlySilent (out.phones[0])) << "mix 0 carries no mix";
		EXPECT_TRUE (exactlySilent (out.phones[1])) << "mix 0 carries no mix";
		EXPECT_EQ (deck->decoderRuns().phones, 0);
	}
}

TEST (DeckSpatial, PhonesAtMixZeroCarryNoMainBus)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnAllStems();
	// cue is pre-mute and the main bus post-mute, so muting every stem removes the main bus alone
	auto phonesWith = [&] (bool muted)
	{
		auto deck = preparedDeck (block, [muted] (DeckSpatial::Controls& c)
		{
			busyDeck (c);
			c.phonesMix = 0.0f;
			c.phonesLevel = 1.0f;
			for (int s = 0; s < DeckSpatial::stems; ++s)
				c.muted[(size_t) s] = muted;
		});
		return render (*deck, in);
	};
	const auto playing = phonesWith (false), muted = phonesWith (true);
	EXPECT_FALSE (exactlySilent (playing.bformat[0], settled)) << "a main bus to keep out";
	EXPECT_FALSE (exactlySilent (playing.phones[0], settled)) << "the cue in the phones";
	for (int side = 0; side < 2; ++side)
		EXPECT_EQ (maxDifference (playing.phones[(size_t) side], muted.phones[(size_t) side], settled), 0.0f) << "mix 0 carries no main bus, side " << side;
}

TEST (DeckSpatial, CueIsTheCuedStemsInIsolation)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto setUp = [] (DeckSpatial::Controls& c)
	{
		c.threeD[0] = 0.6f;
		c.threeD[1] = 0.4f;
		c.azimuth[1] = 90.0f;
		c.cued[1] = true;
	};
	StemInputs both (oneSecond), second (oneSecond);
	both.playNoise (0, 1);
	both.playNoise (1, 2);
	second.playNoise (1, 2);
	auto a = preparedDeck (block, setUp), b = preparedDeck (block, setUp);
	const auto withBoth = render (*a, both), alone = render (*b, second);
	EXPECT_GT (rmsDb (alone.cue[0], settled), -40.0);
	for (size_t c = 0; c < alone.cue.size(); ++c)
		EXPECT_LE (maxDifference (withBoth.cue[c], alone.cue[c]), 1e-6f) << "cue " << c;
}

TEST (DeckSpatial, EnergyIsKeptAcross3D)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	for (int stem = 0; stem < DeckSpatial::stems; ++stem)
	{
		const auto in = noiseOnStem (stem, 7 + stem);
		double levels[3] {};
		const float threeD[3] = { 0.0f, 0.5f, 1.0f };
		for (int k = 0; k < 3; ++k)
		{
			auto deck = preparedDeck (block, [&] (DeckSpatial::Controls& c) { c.threeD[(size_t) stem] = threeD[k]; });
			levels[k] = rmsDb (render (*deck, in).bformat[0], settled);
		}
		EXPECT_NEAR (levels[1], levels[0], 1.5) << "stem " << stem << ", 3D 0.5 vs 0";
		EXPECT_NEAR (levels[2], levels[0], 1.5) << "stem " << stem << ", 3D 1 vs 0";
	}
}

TEST (DeckSpatial, AnyBlockSizeRendersTheSame)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnAllStems();
	auto reference = preparedDeck (block, busyDeck);
	const auto expected = render (*reference, in);
	for (const int size : { 64, 512, 1024, 4096 })
	{
		// calls longer and shorter than the prepared block (chunked) ...
		auto chunked = preparedDeck (block, busyDeck);
		expectAllNear (render (*chunked, in, { size, true, {} }), expected, 1e-5f, "prepared 128, calls of " + std::to_string (size));
		// ... and a deck prepared at that size
		auto prepared = preparedDeck (size, busyDeck);
		expectAllNear (render (*prepared, in, { size, true, {} }), expected, 1e-5f, "prepared and called at " + std::to_string (size));
	}
}

TEST (DeckSpatial, PhonesDecoderOffWhenNotConnected)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	auto deck = preparedDeck (block, [] (DeckSpatial::Controls& c) { busyDeck (c); c.phonesMix = 1.0f; });
	const auto out = render (*deck, noiseOnAllStems(), { block, false, {} });
	EXPECT_FALSE (exactlySilent (out.cue[0])) << "the cue leaves anyway";
	EXPECT_EQ (deck->timing().phonesDecoder, 0.0);
	EXPECT_EQ (deck->decoderRuns().phones, 0);
}

TEST (DeckSpatial, PhonesDecoderOffWhenNotEnabled)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnAllStems();
	for (const float mix : { 1.0f, 0.5f, 0.0f })   // the rig: no decoder, so no phones, cue or mix
	{
		auto deck = preparedDeck (block, [mix] (DeckSpatial::Controls& c) { busyDeck (c); c.phonesDecoderOn = false; c.phonesMix = mix; c.phonesLevel = 1.0f; });
		const auto out = render (*deck, in);
		EXPECT_EQ (deck->timing().phonesDecoder, 0.0) << "mix " << mix;
		EXPECT_EQ (deck->decoderRuns().phones, 0) << "mix " << mix;
		EXPECT_FALSE (exactlySilent (out.cue[0])) << "the cue leaves as B-format, mix " << mix;
		for (int side = 0; side < 2; ++side)
			EXPECT_TRUE (exactlySilent (out.phones[(size_t) side])) << "mix " << mix;
	}
}

TEST (DeckSpatial, PhonesDecoderStopsAtOnceWhenDisconnected)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnAllStems();
	auto deck = preparedDeck (block, [] (DeckSpatial::Controls& c) { busyDeck (c); c.phonesMix = 1.0f; });
	render (*deck, in);
	const auto ranConnected = deck->decoderRuns().phones;
	EXPECT_EQ (ranConnected, (std::int64_t) oneSecond);
	render (*deck, in, { block, false, {} });
	EXPECT_EQ (deck->decoderRuns().phones, ranConnected) << "no hold-over once the phones are gone";
}

TEST (DeckSpatial, PhonesDecoderHoldsOverWhenTurnedOff)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnAllStems();
	const int turnOff = 64 * block;
	auto deck = preparedDeck (block, busyDeck);
	render (*deck, in, { block, true, [&] (int first)
	{
		if (first == turnOff)
		{
			deck->controls().phonesDecoderOn = false;
			deck->resetTiming();
		}
	} });
	const int rampBlocks = (rampSamples + block - 1) / block;
	EXPECT_EQ (deck->decoderRuns().phones, (std::int64_t) (rampBlocks * block + DecoderGate::holdOverSamples));

	deck->resetTiming();
	const auto after = render (*deck, in);
	EXPECT_EQ (deck->decoderRuns().phones, 0);
	EXPECT_EQ (deck->timing().phonesDecoder, 0.0);
	for (int side = 0; side < 2; ++side)
		EXPECT_TRUE (exactlySilent (after.phones[(size_t) side])) << "off, the phones carry nothing, phones " << side;
}

TEST (DeckSpatial, PhonesDecoderReconnectsClean)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnAllStems();
	// mix 1: the phones are the decoded mix alone, so a stale decoder tail would show
	const auto setUp = [] (DeckSpatial::Controls& c) { busyDeck (c); c.phonesMix = 1.0f; };
	auto deck = preparedDeck (block, setUp);
	render (*deck, in);                          // decoding the mix
	render (*deck, in, { block, false, {} });    // unplugged mid-signal: a hard stop

	// Reconnected: while the decoder primes, the phones carry only the cue's share (0 at mix 1).
	const int priming = DecoderGate::holdOverSamples;
	StemInputs primingPart (priming), firstDecoded (block);
	for (size_t c = 0; c < in.channels.size(); ++c)
	{
		std::copy (in.channels[c].begin(), in.channels[c].begin() + priming, primingPart.channels[c].begin());
		std::copy (in.channels[c].begin() + priming, in.channels[c].begin() + priming + block, firstDecoded.channels[c].begin());
	}
	const auto whilePriming = render (*deck, primingPart);
	EXPECT_TRUE (exactlySilent (whilePriming.phones[0]));
	EXPECT_TRUE (exactlySilent (whilePriming.phones[1]));

	const auto reconnected = render (*deck, firstDecoded);
	auto fresh = preparedDeck (block, setUp);
	const auto first = render (*fresh, firstDecoded);
	for (int side = 0; side < 2; ++side)
	{
		EXPECT_FALSE (exactlySilent (first.phones[(size_t) side]));
		EXPECT_LE (maxDifference (reconnected.phones[(size_t) side], first.phones[(size_t) side]), 1e-5f) << "phones " << side;
	}
}

TEST (DeckSpatial, PhonesPrimingNeverRunsMoreThanOneExtraBlockPerCall)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnAllStems();
	auto deck = preparedDeck (block, [] (DeckSpatial::Controls& c) { busyDeck (c); c.phonesMix = 1.0f; });
	render (*deck, in);
	render (*deck, in, { block, false, {} });

	deck->resetTiming();
	std::int64_t previous = 0, mostInOneCall = 0;
	render (*deck, in, { block, true, [&] (int)
	{
		const auto now = deck->decoderRuns().phones;
		mostInOneCall = std::max (mostInOneCall, now - previous);
		previous = now;
	} });
	EXPECT_GT (deck->decoderRuns().phones, (std::int64_t) DecoderGate::holdOverSamples) << "it primed and then decoded";
	EXPECT_LE (mostInOneCall, (std::int64_t) (2 * block));
}

TEST (DeckSpatial, OnePhonesOutputAloneIsClearedNotDecoded)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnAllStems();
	auto deck = preparedDeck (block, [] (DeckSpatial::Controls& c) { busyDeck (c); c.phonesMix = 1.0f; });
	Signal left (oneSecond, 0.5f);   // garbage the deck must overwrite
	for (int offset = 0; offset < oneSecond; offset += block)
	{
		const int count = std::min (block, oneSecond - offset);
		std::array<const float*, 8> stems {};
		for (size_t c = 0; c < stems.size(); ++c)
			stems[c] = in.channels[c].data() + offset;
		DeckSpatial::Outputs outputs;
		outputs.phones[0] = left.data() + offset;
		deck->process (stems.data(), outputs, count);
	}
	EXPECT_TRUE (exactlySilent (left));
	EXPECT_EQ (deck->decoderRuns().phones, 0);
}

TEST (DeckSpatial, AnEmptyCallIsNoBlock)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	auto deck = preparedDeck (block, busyDeck);
	std::array<const float*, 8> stems {};
	deck->process (stems.data(), {}, 0);
	const auto t = deck->timing();
	EXPECT_EQ (t.encoders, 0.0);
	EXPECT_EQ (t.bed, 0.0);
	render (*deck, noiseOnAllStems());
	EXPECT_GT (deck->timing().encoders, 0.0);
}

TEST (DeckSpatial, CueBedStopsWithTheLastCue)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnAllStems();
	const int cueOff = 64 * block;
	auto deck = preparedDeck (block, busyDeck);
	render (*deck, in, { block, true, [&] (int first)
	{
		if (first == cueOff)
			deck->controls().cued[1] = deck->controls().cued[2] = false;
	} });
	deck->resetTiming();
	const auto after = render (*deck, in);
	EXPECT_EQ (deck->timing().cueBed, 0.0);
	for (size_t c = 0; c < after.cue.size(); ++c)
		EXPECT_TRUE (exactlySilent (after.cue[c])) << "cue " << c;
}

TEST (DeckSpatial, CueRestartsClean)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnAllStems();
	const int cueOff = 64 * block, restart = 176 * block;   // off for ~0.25 s, then on again
	auto deck = preparedDeck (block, busyDeck);
	const auto out = render (*deck, in, { block, true, [&] (int first)
	{
		const bool cued = first < cueOff || first >= restart;
		deck->controls().cued[1] = deck->controls().cued[2] = cued;
	} });

	// A fresh deck fed the same block.
	StemInputs oneBlock (block);
	for (size_t c = 0; c < oneBlock.channels.size(); ++c)
		std::copy (in.channels[c].begin() + restart, in.channels[c].begin() + restart + block, oneBlock.channels[c].begin());
	auto fresh = preparedDeck (block, busyDeck);
	const auto first = render (*fresh, oneBlock);
	EXPECT_FALSE (exactlySilent (first.cue[0]));
	for (size_t c = 0; c < first.cue.size(); ++c)
	{
		Signal restarted (out.cue[c].begin() + restart, out.cue[c].begin() + restart + block);
		EXPECT_LE (maxDifference (restarted, first.cue[c]), 1e-5f) << "cue " << c;
	}
}

// R14: a standing deck -- not playing, silent, every ramp settled -- runs no
// encoder and no bed; its bformat and aux are exactly 0.0f.
namespace
{
constexpr int playedBlocks = 30, standingBlocks = 100;
constexpr int standFrom = playedBlocks * block, resumeAt = (playedBlocks + standingBlocks) * block;

// Noise, then silence while the deck stands, then the noise again.
StemInputs playStandPlay()
{
	auto in = noiseOnAllStems();
	for (auto& channel : in.channels)
		std::fill (channel.begin() + standFrom, channel.begin() + resumeAt, 0.0f);
	return in;
}

void expectBformatNearFrom (const Rendered& a, const Rendered& b, int from, float tolerance, const std::string& what)
{
	for (int c = 0; c < spatial::bformatChannels; ++c)
	{
		EXPECT_LE (maxDifference (a.bformat[(size_t) c], b.bformat[(size_t) c], from), tolerance) << what << ", bformat " << c;
		EXPECT_LE (maxDifference (a.aux[(size_t) c], b.aux[(size_t) c], from), tolerance) << what << ", aux " << c;
	}
}

void expectStereoNearFrom (const Rendered& a, const Rendered& b, int from, float tolerance, const std::string& what)
{
	for (size_t c = 0; c < a.cue.size(); ++c)
		EXPECT_LE (maxDifference (a.cue[c], b.cue[c], from), tolerance) << what << ", cue " << c;
	for (int side = 0; side < 2; ++side)
		EXPECT_LE (maxDifference (a.phones[(size_t) side], b.phones[(size_t) side], from), tolerance) << what << ", phones " << side;
}

void movePose (DeckSpatial::Controls& c)
{
	for (int s = 0; s < DeckSpatial::stems; ++s)
	{
		c.azimuth[(size_t) s] = 100.0f - 50.0f * (float) s;
		c.elevation[(size_t) s] = 40.0f - 5.0f * (float) s;
		c.width[(size_t) s] = 80.0f - 15.0f * (float) s;
	}
}
}

TEST (DeckSpatial, AStandingDeckCostsNothingAndIsSilent)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const int stopAt = 50 * block, measureFrom = 60 * block, measuredBlocks = 200;
	StemInputs in (measureFrom + measuredBlocks * block);
	for (int s = 0; s < DeckSpatial::stems; ++s)
		in.playNoise (s, 10 * (s + 1));
	for (auto& channel : in.channels)
		std::fill (channel.begin() + stopAt, channel.end(), 0.0f);

	auto deck = preparedDeck (block, busyDeck);
	const auto out = render (*deck, in, { block, true, [&] (int first)
	{
		if (first == stopAt)
			deck->controls().playing = false;
		if (first == measureFrom)
			deck->resetTiming();
	} });

	const auto t = deck->timing();
	EXPECT_EQ (t.encoders, 0.0);
	EXPECT_EQ (t.bed, 0.0);
	EXPECT_EQ (t.cueBed, 0.0);
	for (int c = 0; c < spatial::bformatChannels; ++c)
	{
		EXPECT_TRUE (exactlySilent (out.bformat[(size_t) c], measureFrom)) << "bformat " << c;
		EXPECT_TRUE (exactlySilent (out.aux[(size_t) c], measureFrom)) << "aux " << c;
	}
	EXPECT_GT (deck->decoderRuns().phones, 0) << "the phones follow R12 unchanged";

	deck->resetTiming();
	render (*deck, StemInputs (8 * block));
	EXPECT_EQ (deck->timing().cueBed, 0.0);
	EXPECT_EQ (deck->timing().encoders, 0.0);
}

TEST (DeckSpatial, StartingAgainSoundsAsNeverStopped)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = playStandPlay();
	auto always = preparedDeck (block, busyDeck);
	auto stood = preparedDeck (block, busyDeck);
	const auto reference = render (*always, in);
	const auto out = render (*stood, in, { block, true, [&] (int first)
	{
		stood->controls().playing = first < standFrom || first >= resumeAt;
		if (first == standFrom + 10 * block)
			stood->resetTiming();
		if (first == resumeAt)
			EXPECT_EQ (stood->timing().encoders, 0.0) << "it did stand";
	} });

	expectBformatNearFrom (out, reference, resumeAt + block, 1e-6f, "from the second resumed block");
	expectBformatNearFrom (out, reference, resumeAt, 1e-6f, "the first resumed block too");
	expectStereoNearFrom (out, reference, resumeAt, 1e-6f, "resumed");
}

TEST (DeckSpatial, APoseChangedWhileStandingIsThereOnResume)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = playStandPlay();
	const int moveAt = standFrom + 50 * block;
	auto always = preparedDeck (block, busyDeck);
	auto stood = preparedDeck (block, busyDeck);
	const auto reference = render (*always, in, { block, true, [&] (int first)
	{
		if (first == moveAt)
			movePose (always->controls());
	} });
	const auto out = render (*stood, in, { block, true, [&] (int first)
	{
		stood->controls().playing = first < standFrom || first >= resumeAt;
		if (first == moveAt)
			movePose (stood->controls());
	} });

	expectBformatNearFrom (out, reference, resumeAt, 1e-5f, "the new pose from the first resumed block");
	for (int c = 0; c < spatial::bformatChannels; ++c)
		EXPECT_TRUE (exactlySilent (out.bformat[(size_t) c], standFrom + block, resumeAt)) << "standing, bformat " << c;
}

TEST (DeckSpatial, APlayingDeckOnSilenceKeepsEncoding)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	auto deck = preparedDeck (block, busyDeck);
	render (*deck, StemInputs (20 * block));
	deck->resetTiming();
	render (*deck, StemInputs (20 * block));
	EXPECT_GT (deck->timing().encoders, 0.0) << "one-shots move during silent passages";
	EXPECT_GT (deck->timing().bed, 0.0);
}

TEST (DeckSpatial, AMovingRampKeepsAStoppedDeckRunning)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	auto deck = preparedDeck (block, [] (DeckSpatial::Controls& c) { busyDeck (c); c.playing = false; });
	render (*deck, StemInputs (20 * block));
	deck->resetTiming();
	deck->controls().muted[0] = true;   // its 10 ms ramp moves for four blocks
	render (*deck, StemInputs (2 * block));
	EXPECT_GT (deck->timing().encoders, 0.0);

	render (*deck, StemInputs (4 * block));
	deck->resetTiming();
	render (*deck, StemInputs (20 * block));
	EXPECT_EQ (deck->timing().encoders, 0.0) << "settled, it stands again";
}

// B1: the per-stem AUX send. The aux bus is the stem's own post-3D, post-MUTE
// encoder output plus its steady share through a third bed, x trim.
namespace
{
void expectAuxEqualsMain (const Rendered& aux, const Rendered& main, float tolerance, int from = settled)
{
	for (int c = 0; c < spatial::bformatChannels; ++c)
		EXPECT_LE (maxDifference (aux.aux[(size_t) c], main.bformat[(size_t) c], from), tolerance) << "channel " << c;
}

StemInputs sineOnAllStemsFrom (int startFrame, int frames)
{
	StemInputs in (frames);
	for (int stem = 0; stem < DeckSpatial::stems; ++stem)
		for (int side = 0; side < 2; ++side)
			for (int i = startFrame; i < frames; ++i)
				in.channels[(size_t) (2 * stem + side)][(size_t) i]
					= 0.25f * std::sin (2.0f * 3.14159265f * 100.0f * (float) (i - startFrame) / (float) rate);
	return in;
}

float largestStep (const Signal& signal, int from, int to)
{
	float worst = 0.0f;
	for (int i = from + 1; i < to; ++i)
		worst = std::max (worst, std::abs (signal[(size_t) i] - signal[(size_t) i - 1]));
	return worst;
}
}

TEST (DeckSpatial, AuxZeroIsExactSilence)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	auto deck = preparedDeck (block, busyDeck);
	const auto out = render (*deck, noiseOnAllStems());
	for (int c = 0; c < spatial::bformatChannels; ++c)
		EXPECT_TRUE (exactlySilent (out.aux[(size_t) c])) << "aux " << c;
	EXPECT_EQ (deck->decoderRuns().auxBed, 0) << "no aux, no aux bed";
}

TEST (DeckSpatial, OneStemSentIsThatStemOnly)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto setPose = [] (DeckSpatial::Controls& c)
	{
		c.threeD[2] = 0.6f;
		c.azimuth[2] = 40.0f;
		c.elevation[2] = 15.0f;
	};
	auto sent = preparedDeck (block, [&] (DeckSpatial::Controls& c) { setPose (c); c.aux[2] = 1.0f; });
	auto alone = preparedDeck (block, setPose);
	const auto aux = render (*sent, noiseOnAllStems());
	const auto mainOfThatStem = render (*alone, noiseOnStem (2, 30));
	expectAuxEqualsMain (aux, mainOfThatStem, 1e-5f);
	EXPECT_GT (rmsDb (aux.aux[0], settled), -60.0);
}

TEST (DeckSpatial, AuxScalesWithTheSend)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnStem (1);
	const auto configure = [] (float aux)
	{
		return [aux] (DeckSpatial::Controls& c) { c.threeD[1] = 0.5f; c.azimuth[1] = -60.0f; c.aux[1] = aux; };
	};
	auto full = preparedDeck (block, configure (1.0f)), half = preparedDeck (block, configure (0.5f));
	const auto a = render (*full, in), b = render (*half, in);
	for (int c = 0; c < spatial::bformatChannels; ++c)
		for (int i = settled; i < oneSecond; ++i)
			ASSERT_NEAR (b.aux[(size_t) c][(size_t) i], 0.5f * a.aux[(size_t) c][(size_t) i], 1e-6f) << "channel " << c;
}

TEST (DeckSpatial, AuxKeepsTheStemsPosition)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnStem (0);
	auto deck = preparedDeck (block, [] (DeckSpatial::Controls& c) { c.threeD[0] = 1.0f; c.azimuth[0] = 90.0f; c.aux[0] = 1.0f; });
	const auto out = render (*deck, in);
	// ACN 1 is Y, ACN 3 is X
	const double mainRatio = std::pow (10.0, (rmsDb (out.bformat[1], settled) - rmsDb (out.bformat[3], settled)) / 20.0);
	const double auxRatio = std::pow (10.0, (rmsDb (out.aux[1], settled) - rmsDb (out.aux[3], settled)) / 20.0);
	EXPECT_GT (mainRatio, 1.0) << "a stem at 90 degrees sits left, so Y dominates";
	EXPECT_NEAR (auxRatio, mainRatio, 1e-4 * mainRatio);
}

TEST (DeckSpatial, MutedStemSendsNoAux)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	auto deck = preparedDeck (block, [] (DeckSpatial::Controls& c) { c.threeD[0] = 0.5f; c.aux[0] = 1.0f; });
	const auto out = render (*deck, noiseOnStem (0), { block, true, [&] (int first)
	{
		deck->controls().muted[0] = first >= oneSecond / 4;
	} });
	const int afterRamp = oneSecond / 4 + block + rampSamples;
	EXPECT_FALSE (exactlySilent (out.aux[0], 0, oneSecond / 4)) << "aux before the mute";
	for (int c = 0; c < spatial::bformatChannels; ++c)
		EXPECT_TRUE (exactlySilent (out.aux[(size_t) c], afterRamp)) << "aux " << c;
}

TEST (DeckSpatial, AuxIsPostThreeD)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnStem (0);
	auto flat = preparedDeck (block, [] (DeckSpatial::Controls& c) { c.threeD[0] = 0.0f; c.aux[0] = 1.0f; });
	expectAuxEqualsMain (render (*flat, in), Rendered { bedReference (in.left (0), in.right (0)), {}, {}, {}, {} }, 1e-6f);

	auto wide = preparedDeck (block, [] (DeckSpatial::Controls& c)
	{
		c.threeD[0] = 1.0f;
		c.azimuth[0] = 30.0f;
		c.width[0] = 90.0f;
		c.aux[0] = 1.0f;
	});
	const auto encoded = encoderReference (in.left (0), in.right (0), { 30.0f, 0.0f, 0.0f, 90.0f });
	const auto aux = render (*wide, in);
	for (int c = 0; c < spatial::bformatChannels; ++c)
		EXPECT_LE (maxDifference (aux.aux[(size_t) c], encoded[(size_t) c], settled), 1e-6f) << "no bed share at 3D 1, channel " << c;
}

TEST (DeckSpatial, StandingDeckRunsNoAuxBed)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = sineOnAllStemsFrom (resumeAt, resumeAt + 4 * block);
	auto deck = preparedDeck (block, [] (DeckSpatial::Controls& c)
	{
		for (int s = 0; s < DeckSpatial::stems; ++s)
			c.aux[(size_t) s] = 1.0f;
	});
	// playing until standFrom (the sine ends there), then standing
	auto played = in;
	for (int ch = 0; ch < 8; ++ch)
		for (int i = 0; i < standFrom; ++i)
			played.channels[(size_t) ch][(size_t) i] = 0.25f * std::sin (2.0f * 3.14159265f * 100.0f * (float) i / (float) rate);
	std::int64_t runsWhileStanding = -1;
	const auto out = render (*deck, played, { block, true, [&] (int first)
	{
		deck->controls().playing = first < standFrom || first >= resumeAt;
		if (first == standFrom + 30 * block)
			deck->resetTiming();   // the 2048-sample hold-over has run out by now
		if (first == resumeAt)
			runsWhileStanding = deck->decoderRuns().auxBed;
	} });
	EXPECT_EQ (runsWhileStanding, 0);
	for (int c = 0; c < spatial::bformatChannels; ++c)
	{
		EXPECT_TRUE (exactlySilent (out.aux[(size_t) c], standFrom + 30 * block, resumeAt)) << "standing, aux " << c;
		EXPECT_LT (largestStep (out.aux[(size_t) c], resumeAt, resumeAt + 4 * block), 0.05f) << "click on resume, aux " << c;
	}
	EXPECT_GT (deck->decoderRuns().auxBed, 0);
}

// B2 (M4): the cue leaves as 2nd-order B-format -- the first 9 ACN channels of
// the 3rd-order cue sum -- and the deck has one decoder, for the phones.
namespace
{
void expectCueIsFirstNineOf (const Rendered& out, const Bformat& thirdOrder, float tolerance)
{
	for (size_t c = 0; c < out.cue.size(); ++c)
		EXPECT_LE (maxDifference (out.cue[c], thirdOrder[c], settled), tolerance) << "cue " << c;
}

Bformat sum (const Bformat& a, const Bformat& b)
{
	Bformat out = a;
	for (size_t c = 0; c < out.size(); ++c)
		for (size_t i = 0; i < out[c].size(); ++i)
			out[c][i] += b[c][i];
	return out;
}
}

TEST (DeckSpatial, CueIsTheFirstNineChannelsOfTheThirdOrderCue)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnStem (0);
	// 3D 1 with a pose, so the higher orders are busy: the encoder alone
	auto encoded = preparedDeck (block, [] (DeckSpatial::Controls& c)
	{
		c.threeD[0] = 1.0f;
		c.azimuth[0] = 30.0f;
		c.elevation[0] = 25.0f;
		c.width[0] = 90.0f;
		c.cued[0] = true;
	});
	const auto reference = encoderReference (in.left (0), in.right (0), { 30.0f, 25.0f, 0.0f, 90.0f });
	const auto out = render (*encoded, in);
	EXPECT_GT (rmsDb (out.cue[8], settled), -60.0) << "ACN 8 carries the pose";
	expectCueIsFirstNineOf (out, reference, 1e-6f);

	// 3D 0: the cue bed alone
	auto flat = preparedDeck (block, [] (DeckSpatial::Controls& c) { c.cued[0] = true; });
	expectCueIsFirstNineOf (render (*flat, in), bedReference (in.left (0), in.right (0)), 1e-6f);
}

// SD section 5: the cue is taken before MUTE, after GAIN and 3D (stem and deck).
TEST (DeckSpatial, CueIsPreMutePostThreeD)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnStem (0);
	const auto bed = bedReference (in.left (0), in.right (0));
	const auto encoded = encoderReference (in.left (0), in.right (0), { -60.0f, 0.0f, 0.0f, 60.0f });
	const auto scaledBy = [] (const Bformat& from, float gain)
	{
		Bformat out = from;
		for (auto& channel : out)
			for (auto& sample : channel)
				sample *= gain;
		return out;
	};
	// Encoder and bed are linear in their input: 3D 0.5 is g of the one plus 1 - g of the other.
	const float g = spatial::movingGain (0.5f);
	const auto halfEach = sum (scaledBy (encoded, g), scaledBy (bed, 1.0f - g));

	const auto mutedAndCued = [] (float stemThreeD, float deckThreeD)
	{
		return [=] (DeckSpatial::Controls& c)
		{
			c.threeD[0] = stemThreeD;
			c.deckThreeD = deckThreeD;
			c.azimuth[0] = -60.0f;
			c.cued[0] = true;
			c.muted[0] = true;
		};
	};
	auto atHalf = preparedDeck (block, mutedAndCued (0.5f, 1.0f));
	const auto out = render (*atHalf, in);
	for (int c = 0; c < spatial::bformatChannels; ++c)
		EXPECT_TRUE (exactlySilent (out.bformat[(size_t) c])) << "muted, bformat " << c;
	expectCueIsFirstNineOf (out, halfEach, 1e-5f);

	auto deckFlat = preparedDeck (block, mutedAndCued (1.0f, 0.0f));
	expectCueIsFirstNineOf (render (*deckFlat, in), bed, 1e-6f);
}

// Silence at 0 (P4 D23): never cued, or uncued once the 10 ms ramp has
// settled, every cue channel is exactly 0.0f and the cue bed is not run.
TEST (DeckSpatial, NoStemCuedIsExactSilenceOnTheCue)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnAllStems();
	auto never = preparedDeck (block, [] (DeckSpatial::Controls& c) { busyDeck (c); c.cued[1] = c.cued[2] = false; });
	const auto out = render (*never, in);
	for (size_t c = 0; c < out.cue.size(); ++c)
		EXPECT_TRUE (exactlySilent (out.cue[c])) << "never cued, cue " << c;
	EXPECT_EQ (never->timing().cueBed, 0.0);

	const int cueOff = 64 * block;
	auto deck = preparedDeck (block, busyDeck);
	const auto uncued = render (*deck, in, { block, true, [&] (int first)
	{
		if (first == cueOff)
			deck->controls().cued[1] = deck->controls().cued[2] = false;
	} });
	EXPECT_FALSE (exactlySilent (uncued.cue[0], 0, cueOff));
	for (size_t c = 0; c < uncued.cue.size(); ++c)
		EXPECT_TRUE (exactlySilent (uncued.cue[c], cueOff + block + rampSamples)) << "uncued, cue " << c;
}

TEST (DeckSpatial, OnlyOneDecoderExists)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const int before = DeckSpatial::binauralDecodersCreated();
	auto deck = preparedDeck();
	EXPECT_EQ (DeckSpatial::binauralDecodersCreated() - before, 1);
}

// The phones decoder takes mixWeight * main16 + cueWeight * cue9 (ACN 9..15 zero):
// at either end of CUE<->MIX it is a lone decoder on that one input.
TEST (DeckSpatial, PhonesMixesMainAndNinthOrderCue)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnAllStems();
	for (const float mix : { 0.0f, 1.0f })
	{
		auto deck = preparedDeck (block, [mix] (DeckSpatial::Controls& c) { busyDeck (c); c.phonesMix = mix; c.phonesLevel = 1.0f; });
		const auto out = render (*deck, in);
		const auto reference = decoderReference (mix == 0.0f ? padded (out.cue) : out.bformat);
		for (size_t side = 0; side < 2; ++side)
		{
			EXPECT_GT (rmsDb (out.phones[side], settled), -40.0) << "mix " << mix;
			EXPECT_LE (maxDifference (out.phones[side], reference[side]), 1e-5f) << "mix " << mix << ", phones " << side;
		}
	}
}

// R2: two-deck mode's binaural cue is the phones decoder at cue 1, mix 0 --
// whatever the phones settings say -- and its phones pair carries the same.
TEST (DeckSpatial, TwoDeckBinauralCueEqualsPhonesCueOnly)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnAllStems();
	auto twoDeck = preparedDeck (block, [] (DeckSpatial::Controls& c) { busyDeck (c); c.phonesDecoderOn = false; c.phonesMix = 0.7f; c.phonesLevel = 0.5f; });
	auto cueOnly = preparedDeck (block, [] (DeckSpatial::Controls& c) { busyDeck (c); c.phonesMix = 0.0f; c.phonesLevel = 1.0f; });
	RenderOptions withBinauralCue;
	withBinauralCue.binauralCue = true;
	const auto two = render (*twoDeck, in, withBinauralCue);
	const auto one = render (*cueOnly, in);
	for (size_t side = 0; side < 2; ++side)
	{
		EXPECT_GT (rmsDb (two.binauralCue[side], settled), -40.0) << side;
		EXPECT_LE (maxDifference (two.binauralCue[side], one.phones[side], settled), 1e-6f) << "bcue " << side;
		EXPECT_EQ (maxDifference (two.phones[side], two.binauralCue[side]), 0.0f) << "phones " << side;
	}

	// Nothing cued: the decoder does not run and the pair is exactly 0.0f.
	auto idle = preparedDeck (block, [] (DeckSpatial::Controls& c) { busyDeck (c); c.phonesDecoderOn = false; c.cued[1] = c.cued[2] = false; });
	const auto silent = render (*idle, in, withBinauralCue);
	EXPECT_EQ (idle->decoderRuns().phones, 0);
	for (size_t side = 0; side < 2; ++side)
		EXPECT_TRUE (exactlySilent (silent.binauralCue[side])) << side;
}
