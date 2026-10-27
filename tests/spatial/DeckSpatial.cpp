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
		EXPECT_LE (maxDifference (a.bformat[(size_t) c], b.bformat[(size_t) c]), tolerance) << what << ", bformat " << c;
	for (int side = 0; side < 2; ++side)
	{
		EXPECT_LE (maxDifference (a.cue[(size_t) side], b.cue[(size_t) side]), tolerance) << what << ", cue " << side;
		EXPECT_LE (maxDifference (a.aux[(size_t) side], b.aux[(size_t) side]), tolerance) << what << ", aux " << side;
		EXPECT_LE (maxDifference (a.phones[(size_t) side], b.phones[(size_t) side]), tolerance) << what << ", phones " << side;
	}
}

void expectEverythingSilent (const Rendered& out, int from, const std::string& what)
{
	for (int c = 0; c < spatial::bformatChannels; ++c)
		EXPECT_TRUE (exactlySilent (out.bformat[(size_t) c], from)) << what << ", bformat " << c;
	for (int side = 0; side < 2; ++side)
	{
		EXPECT_TRUE (exactlySilent (out.cue[(size_t) side], from)) << what << ", cue " << side;
		EXPECT_TRUE (exactlySilent (out.aux[(size_t) side], from)) << what << ", aux " << side;
		EXPECT_TRUE (exactlySilent (out.phones[(size_t) side], from)) << what << ", phones " << side;
	}
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

TEST (DeckSpatial, MuteSilencesMainAndAuxButNotCue)
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
	for (int side = 0; side < 2; ++side)
	{
		EXPECT_FALSE (exactlySilent (out.aux[(size_t) side], 0, oneSecond / 4)) << "aux before the mute";
		EXPECT_TRUE (exactlySilent (out.aux[(size_t) side], afterRamp)) << "aux " << side;
		EXPECT_GE (rmsDb (out.cue[(size_t) side], afterRamp), -30.0) << "cue " << side;
	}
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
		// mix 0: phones are the cue alone
		auto deck = preparedDeck (block, [] (DeckSpatial::Controls& c) { busyDeck (c); c.phonesMix = 0.0f; c.phonesLevel = 1.0f; });
		const auto out = render (*deck, in);
		for (int side = 0; side < 2; ++side)
			EXPECT_EQ (maxDifference (out.phones[(size_t) side], out.cue[(size_t) side]), 0.0f) << "mix 0 carries no mix";
	}
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
	for (int side = 0; side < 2; ++side)
	{
		EXPECT_GT (rmsDb (alone.cue[(size_t) side], settled), -40.0);
		EXPECT_LE (maxDifference (withBoth.cue[(size_t) side], alone.cue[(size_t) side]), 1e-5f) << "cue " << side;
	}
}

TEST (DeckSpatial, NothingCuedIsExactSilence)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	auto deck = preparedDeck (block, [] (DeckSpatial::Controls& c) { busyDeck (c); c.cued[1] = c.cued[2] = false; });
	const auto out = render (*deck, noiseOnAllStems());
	EXPECT_TRUE (exactlySilent (out.cue[0]));
	EXPECT_TRUE (exactlySilent (out.cue[1]));
	EXPECT_EQ (deck->decoderRuns().cue, 0);
}

TEST (DeckSpatial, AuxIsTheStereoSum)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnAllStems();
	auto deck = preparedDeck (block, busyDeck);
	const auto out = render (*deck, in);
	for (int side = 0; side < 2; ++side)
	{
		Signal sum = silence (oneSecond);
		for (int stem = 0; stem < DeckSpatial::stems; ++stem)
		{
			const auto& x = side == 0 ? in.left (stem) : in.right (stem);
			for (int i = 0; i < oneSecond; ++i)
				sum[(size_t) i] += x[(size_t) i] * spatial::trim;
		}
		EXPECT_LE (maxDifference (out.aux[(size_t) side], sum), 1e-6f) << "aux " << side;
	}
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

TEST (DeckSpatial, PhonesAtMixZeroAreTheCue)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	auto deck = preparedDeck (block, [] (DeckSpatial::Controls& c) { busyDeck (c); c.phonesMix = 0.0f; c.phonesLevel = 1.0f; });
	const auto out = render (*deck, noiseOnAllStems());
	for (int side = 0; side < 2; ++side)
	{
		EXPECT_FALSE (exactlySilent (out.cue[(size_t) side]));
		EXPECT_LE (maxDifference (out.phones[(size_t) side], out.cue[(size_t) side]), 1e-6f);
	}
	EXPECT_EQ (deck->decoderRuns().phones, 0) << "no mix share, no phones decoder";
}

TEST (DeckSpatial, PhonesDecoderOffWhenNotConnected)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	auto deck = preparedDeck (block, [] (DeckSpatial::Controls& c) { busyDeck (c); c.phonesMix = 1.0f; });
	render (*deck, noiseOnAllStems(), { block, false, {} });
	EXPECT_EQ (deck->timing().phonesDecoder, 0.0);
	EXPECT_EQ (deck->decoderRuns().phones, 0);
	EXPECT_GT (deck->timing().cueDecoder, 0.0) << "the cue decoder still runs";
}

TEST (DeckSpatial, PhonesDecoderOffWhenNotEnabled)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnAllStems();
	for (const float mix : { 1.0f, 0.5f })   // at 1 the cue's share is 0: silence; at 0.5 the cue at its weight
	{
		auto deck = preparedDeck (block, [mix] (DeckSpatial::Controls& c) { busyDeck (c); c.phonesDecoderOn = false; c.phonesMix = mix; c.phonesLevel = 1.0f; });
		const auto out = render (*deck, in);
		EXPECT_EQ (deck->timing().phonesDecoder, 0.0) << "mix " << mix;
		EXPECT_EQ (deck->decoderRuns().phones, 0) << "mix " << mix;
		const float cueWeight = spatial::phonesWeights (1.0f, mix).cue;
		for (int side = 0; side < 2; ++side)
		{
			Signal cueShare (out.cue[(size_t) side]);
			for (auto& sample : cueShare)
				sample *= cueWeight;
			EXPECT_EQ (exactlySilent (cueShare), mix == 1.0f) << "mix " << mix;
			EXPECT_LE (maxDifference (out.phones[(size_t) side], cueShare), 1e-6f) << "only the cue weight's share, mix " << mix;
		}
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
	const float cueWeight = spatial::phonesWeights (0.8f, 0.5f).cue;
	for (int side = 0; side < 2; ++side)
	{
		Signal cueShare (after.cue[(size_t) side]);
		for (auto& sample : cueShare)
			sample *= cueWeight;
		EXPECT_EQ (maxDifference (after.phones[(size_t) side], cueShare), 0.0f) << "no mix share, phones " << side;
	}
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

TEST (DeckSpatial, CueDecoderStopsAfterTheHoldOver)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const auto in = noiseOnAllStems();
	const int cueOff = 64 * block;
	auto deck = preparedDeck (block, busyDeck);
	render (*deck, in, { block, true, [&] (int first)
	{
		if (first == cueOff)
		{
			deck->controls().cued[1] = deck->controls().cued[2] = false;
			deck->resetTiming();
		}
	} });
	// the ramp out, in whole blocks, then the hold-over
	const int rampBlocks = (rampSamples + block - 1) / block;
	EXPECT_EQ (deck->decoderRuns().cue, (std::int64_t) (rampBlocks * block + DecoderGate::holdOverSamples));

	deck->resetTiming();
	const auto after = render (*deck, in);
	EXPECT_EQ (deck->timing().cueDecoder, 0.0);
	EXPECT_EQ (deck->timing().cueBed, 0.0);
	EXPECT_EQ (deck->decoderRuns().cue, 0);
	EXPECT_TRUE (exactlySilent (after.cue[0]));
	EXPECT_TRUE (exactlySilent (after.cue[1]));
}

TEST (DeckSpatial, CueDecoderRestartsClean)
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
	for (int side = 0; side < 2; ++side)
	{
		Signal restarted (out.cue[(size_t) side].begin() + restart, out.cue[(size_t) side].begin() + restart + block);
		EXPECT_FALSE (exactlySilent (first.cue[(size_t) side]));
		EXPECT_LE (maxDifference (restarted, first.cue[(size_t) side]), 1e-5f) << "cue " << side;
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
		EXPECT_LE (maxDifference (a.bformat[(size_t) c], b.bformat[(size_t) c], from), tolerance) << what << ", bformat " << c;
}

void expectStereoNearFrom (const Rendered& a, const Rendered& b, int from, float tolerance, const std::string& what)
{
	for (int side = 0; side < 2; ++side)
	{
		EXPECT_LE (maxDifference (a.cue[(size_t) side], b.cue[(size_t) side], from), tolerance) << what << ", cue " << side;
		EXPECT_LE (maxDifference (a.aux[(size_t) side], b.aux[(size_t) side], from), tolerance) << what << ", aux " << side;
		EXPECT_LE (maxDifference (a.phones[(size_t) side], b.phones[(size_t) side], from), tolerance) << what << ", phones " << side;
	}
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
		EXPECT_TRUE (exactlySilent (out.bformat[(size_t) c], measureFrom)) << "bformat " << c;
	for (int side = 0; side < 2; ++side)
		EXPECT_TRUE (exactlySilent (out.aux[(size_t) side], measureFrom)) << "aux " << side;
	EXPECT_GT (deck->decoderRuns().phones, 0) << "the phones follow R12 unchanged";

	// The cue decoder finished its hold-over on silence and stopped.
	deck->resetTiming();
	render (*deck, StemInputs (8 * block));
	EXPECT_EQ (deck->decoderRuns().cue, 0);
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
