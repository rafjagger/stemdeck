// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneDeckAudio.h"

#include "AudioCpu.h"

namespace
{
constexpr int maxBlock = 4096;   // as the deck's own buffers
}

OneDeckAudio::OneDeckAudio (juce::AudioFormatManager& formats, juce::ThreadPool& pool, AnalysisCache& cache)
	: deck (formats, pool, cache, outputs::oneDeck::layout())
{
}

void OneDeckAudio::updatePhonesDecoder (spatial::PhonesMode mode, bool phonesConnected, bool coreHeard)
{
	deck.spatialPorts().settings().phonesDecoderOn.store (spatial::phonesDecoderRuns (mode, phonesConnected, coreHeard));
}

bool OneDeckAudio::phonesDecoderOn() const
{
	return deck.spatialPorts().settings().phonesDecoderOn.load();
}

void OneDeckAudio::setPhones (float level, float mix)
{
	auto& settings = deck.spatialPorts().settings();
	settings.phonesLevel.store (level);
	settings.phonesMix.store (mix);
}

void OneDeckAudio::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
	rendered.setSize (outputs::oneDeck::channelCount(), juce::jmax (samplesPerBlockExpected, maxBlock));
	deck.prepare (sampleRate, samplesPerBlockExpected);
}

void OneDeckAudio::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
	pinAudioThreadToItsCpu();

	const auto numSamples = bufferToFill.numSamples;
	if (numSamples > rendered.getNumSamples())
		rendered.setSize (outputs::oneDeck::channelCount(), numSamples, false, false, true);

	deck.render (rendered, 0, numSamples);

	auto& out = *bufferToFill.buffer;
	bufferToFill.clearActiveBufferRegion();
	for (int ch = 0; ch < outputs::oneDeck::channelCount(); ++ch)
		if (const auto target = outputs::oneDeck::foldsInto (ch, out.getNumChannels()); target >= 0)
			out.addFrom (target, bufferToFill.startSample, rendered, ch, 0, numSamples);
}
