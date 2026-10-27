// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "DeckSpatial.h"
#include "SpatialLaw.h"
#include "SteadyBed.h"

#include <IemNode.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

// Seeded noise through a DeckSpatial in calls of any size, and the references
// its outputs are held against: a lone SteadyBed and a lone StereoEncoder.
namespace sdtest
{
constexpr double rate = 44100.0;
constexpr int block = 128;
constexpr int oneSecond = 44100;
constexpr int settled = 4410;   // 100 ms: every 10 ms gain ramp has arrived

using Signal = std::vector<float>;

inline Signal silence (int frames) { return Signal ((size_t) frames, 0.0f); }

// Uniform noise peaking at -12 dBFS.
inline Signal noise (int frames, int seed)
{
	juce::Random random (seed);
	Signal signal ((size_t) frames);
	for (auto& sample : signal)
		sample = 0.25f * (2.0f * random.nextFloat() - 1.0f);
	return signal;
}

struct StemInputs
{
	explicit StemInputs (int frames)
	{
		for (auto& channel : channels)
			channel = silence (frames);
	}

	void playNoise (int stem, int seed)
	{
		const auto frames = (int) channels[0].size();
		channels[(size_t) (2 * stem)] = noise (frames, seed);
		channels[(size_t) (2 * stem + 1)] = noise (frames, seed + 1000);
	}

	const Signal& left (int stem) const { return channels[(size_t) (2 * stem)]; }
	const Signal& right (int stem) const { return channels[(size_t) (2 * stem + 1)]; }
	int frames() const { return (int) channels[0].size(); }

	std::array<Signal, 8> channels;
};

struct Rendered
{
	std::array<Signal, spatial::bformatChannels> bformat, aux;
	std::array<Signal, 2> cue, phones;
};

struct RenderOptions
{
	int callSize = block;
	bool phones = true;
	std::function<void (int firstSample)> beforeCall;   // may change the controls
};

inline Rendered render (DeckSpatial& deck, const StemInputs& in, const RenderOptions& options = {})
{
	const int frames = in.frames();
	Rendered out;
	for (auto* bus : { &out.bformat, &out.aux })
		for (auto& channel : *bus)
			channel = silence (frames);
	for (auto* pair : { &out.cue, &out.phones })
		for (auto& channel : *pair)
			channel = silence (frames);

	for (int offset = 0; offset < frames; offset += options.callSize)
	{
		const int count = std::min (options.callSize, frames - offset);
		if (options.beforeCall)
			options.beforeCall (offset);
		std::array<const float*, 8> stems {};
		for (size_t c = 0; c < stems.size(); ++c)
			stems[c] = in.channels[c].data() + offset;
		DeckSpatial::Outputs outputs;
		for (size_t c = 0; c < outputs.bformat.size(); ++c)
		{
			outputs.bformat[c] = out.bformat[c].data() + offset;
			outputs.aux[c] = out.aux[c].data() + offset;
		}
		for (size_t side = 0; side < 2; ++side)
		{
			outputs.cue[side] = out.cue[side].data() + offset;
			outputs.phones[side] = options.phones ? out.phones[side].data() + offset : nullptr;
		}
		deck.process (stems.data(), outputs, count);
	}
	return out;
}

inline std::unique_ptr<DeckSpatial> preparedDeck (int blockSize = block, const std::function<void (DeckSpatial::Controls&)>& setUp = {})
{
	juce::String error;
	auto deck = DeckSpatial::create (error);
	if (deck == nullptr)
		throw std::runtime_error ("DeckSpatial::create: " + error.toStdString());
	if (setUp)
		setUp (deck->controls());
	deck->prepare (rate, blockSize);
	return deck;
}

inline float maxDifference (const Signal& a, const Signal& b, int from = 0, int to = -1)
{
	if (to < 0)
		to = (int) a.size();
	float worst = 0.0f;
	for (int i = from; i < to; ++i)
		worst = std::max (worst, std::abs (a[(size_t) i] - b[(size_t) i]));
	return worst;
}

// Not one sample other than 0.0f.
inline bool exactlySilent (const Signal& signal, int from = 0, int to = -1)
{
	if (to < 0)
		to = (int) signal.size();
	for (int i = from; i < to; ++i)
		if (signal[(size_t) i] != 0.0f)
			return false;
	return true;
}

inline double rmsDb (const Signal& signal, int from = 0)
{
	double sum = 0.0;
	for (size_t i = (size_t) from; i < signal.size(); ++i)
		sum += (double) signal[i] * signal[i];
	return 10.0 * std::log10 (sum / (double) (signal.size() - (size_t) from) + 1e-30);
}

inline std::array<Signal, spatial::bformatChannels> scaled (const juce::AudioBuffer<float>& from, int frames, float gain)
{
	std::array<Signal, spatial::bformatChannels> out;
	for (int c = 0; c < spatial::bformatChannels; ++c)
	{
		out[(size_t) c] = silence (frames);
		for (int i = 0; i < frames; ++i)
			out[(size_t) c][(size_t) i] = from.getSample (c, i) * gain;
	}
	return out;
}

// A lone SteadyBed on one stereo pair, x trim.
inline std::array<Signal, spatial::bformatChannels> bedReference (const Signal& left, const Signal& right)
{
	juce::String error;
	auto bed = SteadyBed::create (error);
	if (bed == nullptr)
		throw std::runtime_error ("SteadyBed::create: " + error.toStdString());
	bed->prepare (rate, block);
	const int frames = (int) left.size();
	juce::AudioBuffer<float> all (spatial::bformatChannels, frames), chunk (spatial::bformatChannels, block);
	for (int offset = 0; offset < frames; offset += block)
	{
		const int count = std::min (block, frames - offset);
		bed->process (left.data() + offset, right.data() + offset, chunk, count);
		for (int c = 0; c < spatial::bformatChannels; ++c)
			all.copyFrom (c, offset, chunk, c, 0, count);
	}
	return scaled (all, frames, spatial::trim);
}

// A lone StereoEncoder at a pose, x trim; the pose is set before prepare, as DeckSpatial does.
inline std::array<Signal, spatial::bformatChannels> encoderReference (const Signal& left, const Signal& right, const a3::StereoPose& pose)
{
	auto encoder = a3::IemNode::create (a3::IemKind::stereoEncoder, 2, spatial::bformatChannels);
	if (encoder == nullptr)
		throw std::runtime_error ("no StereoEncoder");
	encoder->setStereoPose (pose);
	encoder->prepare (rate, block);
	const int frames = (int) left.size();
	juce::AudioBuffer<float> all (spatial::bformatChannels, frames), io (spatial::bformatChannels, block);
	for (int offset = 0; offset < frames; offset += block)
	{
		const int count = std::min (block, frames - offset);
		juce::AudioBuffer<float> view (io.getArrayOfWritePointers(), spatial::bformatChannels, count);
		view.clear();
		view.copyFrom (0, 0, left.data() + offset, count);
		view.copyFrom (1, 0, right.data() + offset, count);
		encoder->process (view);
		for (int c = 0; c < spatial::bformatChannels; ++c)
			all.copyFrom (c, offset, io, c, 0, count);
	}
	return scaled (all, frames, spatial::trim);
}
}
