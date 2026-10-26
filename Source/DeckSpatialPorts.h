// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "DeckSpatial.h"

#include <memory>

class StemDeckPlayer;

// One deck's spatial ports in two-deck mode (stage S2): its DeckSpatial fed
// from the player's pre-mute block, written to the deck's 20 output channels
// (Outputs.h) -- B-format, the binaural cue, and phones carrying that cue.
// Mute and cue follow the player's stems each block; deck 3D stays 1 and
// StemDeck's own phones decoder off (no phones UI here). AUX is not rendered:
// the AUX bus stays the AUX.
class DeckSpatialPorts
{
public:
	DeckSpatialPorts();
	~DeckSpatialPorts();

	// Builds the IEM nodes; returns an error and renders silence if IEM refuses.
	juce::String create();
	void prepare (double sampleRate, int blockSize);   // allocates
	// Audio thread: writes channels [firstChannel, firstChannel + 20) of `outputs`.
	void render (const StemDeckPlayer&, juce::AudioBuffer<float>& outputs, int firstChannel, int numSamples);
	DeckSpatial* deckSpatial() const { return spatial.get(); }

private:
	void followPlayer (const StemDeckPlayer&);
	void process (const float* const* stems, juce::AudioBuffer<float>& outputs, int firstChannel, int offset, int count);

	std::unique_ptr<DeckSpatial> spatial;
	juce::AudioBuffer<float> silence;   // fed while the deck gives no pre-mute block, so tails and ramps run out
};
