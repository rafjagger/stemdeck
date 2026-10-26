// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include "DeckHost.h"
#include "Outputs.h"
#include "SpatialLaw.h"

// The audio of a -bformat instance (spec sections 1, 8): one deck, its 22
// outputs (outputs::oneDeck) and nothing else -- no buses, no mixer. Its
// JACK client pulls it as it is; without JACK only what fits the device
// plays (outputs::oneDeck::foldsInto).
class OneDeckAudio : public juce::AudioSource
{
public:
	OneDeckAudio (juce::AudioFormatManager&, juce::ThreadPool& analysisPool, AnalysisCache&);

	DeckHost& host() { return deck; }
	const DeckHost& host() const { return deck; }
	juce::String createSpatial() { return deck.createSpatial(); }

	// Message thread. R12: StemDeck's own phones decoder runs only where
	// spatial::phonesDecoderRuns says.
	void updatePhonesDecoder (spatial::PhonesMode, bool phonesConnected, bool coreHeard);
	bool phonesDecoderOn() const;
	// LEVEL 0..1 and CUE<->MIX 0 (cue) .. 1 (mix).
	void setPhones (float level, float mix);

	void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
	void getNextAudioBlock (const juce::AudioSourceChannelInfo&) override;
	void releaseResources() override { deck.releaseResources(); }

private:
	DeckHost deck;
	juce::AudioBuffer<float> rendered;   // the 22 channels
};
