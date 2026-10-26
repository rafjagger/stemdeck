// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "DeckSpatialPorts.h"

#include "Outputs.h"
#include "StemDeckPlayer.h"

namespace
{
constexpr int maxSilenceBlock = 4096;   // as the player's cue room
}

DeckSpatialPorts::DeckSpatialPorts() = default;
DeckSpatialPorts::~DeckSpatialPorts() = default;

juce::String DeckSpatialPorts::create()
{
	juce::String error;
	spatial = DeckSpatial::create (error);
	if (spatial == nullptr)
		return error.isNotEmpty() ? error : juce::String ("the spatial engine could not be built");

	auto& controls = spatial->controls();
	controls.deckThreeD = 1.0f;
	controls.phonesLevel = 1.0f;
	controls.phonesMix = 0.0f;
	controls.phonesDecoderOn = false;
	return {};
}

void DeckSpatialPorts::prepare (double sampleRate, int blockSize)
{
	silence.setSize (StemDeckPlayer::numOutputChannels, juce::jmax (blockSize, maxSilenceBlock));
	silence.clear();
	if (spatial != nullptr)
		spatial->prepare (sampleRate, blockSize);
}

void DeckSpatialPorts::followPlayer (const StemDeckPlayer& player)
{
	auto& controls = spatial->controls();
	for (int s = 0; s < DeckSpatial::stems; ++s)
	{
		controls.muted[(size_t) s].store (player.isStemMuted (s), std::memory_order_relaxed);
		controls.cued[(size_t) s].store (player.isStemCued (s), std::memory_order_relaxed);
	}
	controls.playing.store (player.isPlaying(), std::memory_order_relaxed);   // stopped and silent, it stands (R14)
}

void DeckSpatialPorts::render (const StemDeckPlayer& player, juce::AudioBuffer<float>& outputs, int firstChannel, int numSamples)
{
	jassert (firstChannel + outputs::spatialChannelsPerDeck() <= outputs.getNumChannels());
	if (spatial == nullptr)
	{
		for (int c = 0; c < outputs::spatialChannelsPerDeck(); ++c)
			outputs.clear (firstChannel + c, 0, numSamples);
		return;
	}

	followPlayer (player);
	if (player.getPreMuteBlockSamples() == numSamples)
	{
		process (player.getPreMuteBlock().getArrayOfReadPointers(), outputs, firstChannel, 0, numSamples);
		return;
	}
	for (int offset = 0; offset < numSamples; offset += silence.getNumSamples())
		process (silence.getArrayOfReadPointers(), outputs, firstChannel, offset,
		         juce::jmin (silence.getNumSamples(), numSamples - offset));
}

void DeckSpatialPorts::process (const float* const* stems, juce::AudioBuffer<float>& outputs, int firstChannel, int offset, int count)
{
	const auto at = [&] (int within) { return outputs.getWritePointer (firstChannel + within) + offset; };
	DeckSpatial::Outputs ports;
	for (int c = 0; c < outputs::bformatChannels; ++c)
		ports.bformat[(size_t) c] = at (outputs::bformatOffset + c);
	for (int side = 0; side < 2; ++side)
	{
		ports.cue[(size_t) side] = at (outputs::binauralCueOffset + side);
		ports.phones[(size_t) side] = at (outputs::phonesOffset + side);
	}
	spatial->process (stems, ports, count);
}
