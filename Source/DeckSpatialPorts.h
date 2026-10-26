// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "DeckMotion.h"
#include "DeckSpatial.h"

#include <memory>

class StemDeckPlayer;

// One deck's spatial ports in two-deck mode (stage S2): its DeckSpatial fed
// from the player's pre-mute block, written to the deck's 20 output channels
// (Outputs.h) -- B-format, the binaural cue, and phones carrying that cue.
// Mute, cue and transport follow the player each block; deck 3D stays 1 and
// StemDeck's own phones decoder off (no phones UI here). AUX is not rendered:
// the AUX bus stays the AUX. Each stem's 3D, home and width are the settings
// (the call-out and the session write them); the one-shots (DeckMotion) move
// the played pose from that home in the player's beat time, every block.
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
	DeckSpatial::Controls& settings() { return userSettings; }
	const DeckSpatial::Controls& settings() const { return userSettings; }
	DeckMotion& motion() { return stemMotion; }
	const DeckMotion& motion() const { return stemMotion; }

private:
	void followPlayer (const StemDeckPlayer&, int numSamples);
	void copySettings (bool withPose);
	void process (const float* const* stems, juce::AudioBuffer<float>& outputs, int firstChannel, int offset, int count);

	DeckSpatial::Controls userSettings;
	DeckMotion stemMotion;
	std::unique_ptr<DeckSpatial> spatial;
	double sampleRate = 44100.0;
	juce::AudioBuffer<float> silence;   // fed while the deck gives no pre-mute block, so tails and ramps run out
};
