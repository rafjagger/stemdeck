// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "DeckSpatialFixture.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <ostream>
#include <string>
#include <vector>

// The CPU gate's measurement, shared by the timing test and the standalone
// tool: one deck with its stems moving -- one stem or all four playing, the cue
// leaving as B-format with no decoder (the rig) or the phones decoder on
// (standalone) -- and a standing deck.
namespace sdtest
{
struct TimingRow
{
	std::string name;
	double totalMean = 0.0, totalP99 = 0.0;   // µs per 128-block, whole process() call
	DeckSpatial::Timing nodes {};              // mean µs per node group
};
using TimingTable = std::array<TimingRow, 6>;

namespace detail
{
inline double p99 (std::vector<double>& samples)
{
	std::sort (samples.begin(), samples.end());
	return samples[(size_t) ((double) (samples.size() - 1) * 0.99)];
}

// All stems move with a new pose every block, so the IEM encoders recompute their gains each time.
inline void movePoses (DeckSpatial::Controls& c, int blockIndex)
{
	for (int s = 0; s < DeckSpatial::stems; ++s)
	{
		const float phase = 0.37f * (float) blockIndex + 1.3f * (float) s;
		c.azimuth[(size_t) s] = std::fmod (phase * 7.0f, 360.0f) - 180.0f;
		c.elevation[(size_t) s] = 30.0f + 25.0f * std::sin (phase * 0.1f);
	}
}

enum class Deck { standing, playing };

inline TimingRow measure (const char* name, int blocks, bool cued, bool phones, Deck deckState = Deck::playing, int playingStems = DeckSpatial::stems)
{
	const bool standing = deckState == Deck::standing;
	auto deck = preparedDeck (block, [&] (DeckSpatial::Controls& c)
	{
		for (int s = 0; s < DeckSpatial::stems; ++s)
		{
			c.envelope[(size_t) s] = 1.0f;
			c.cued[(size_t) s] = cued && s < playingStems;
		}
		c.phonesDecoderOn = phones;
		c.phonesMix = 0.5f;
		c.phonesLevel = 1.0f;
		c.playing = ! standing;
	});

	StemInputs in (block);
	if (! standing)   // standing: silent, poses still (one-shots freeze while paused)
		for (int s = 0; s < playingStems; ++s)
			in.playNoise (s, 10 * (s + 1));
	std::array<const float*, 8> stems {};
	for (size_t c = 0; c < stems.size(); ++c)
		stems[c] = in.channels[c].data();

	std::array<Signal, spatial::bformatChannels> bformat, aux;
	std::array<Signal, DeckSpatial::cueChannels> cue;
	std::array<Signal, 2> phonesPair;
	DeckSpatial::Outputs outputs;
	for (size_t c = 0; c < bformat.size(); ++c)
	{
		outputs.bformat[c] = (bformat[c] = silence (block)).data();
		outputs.aux[c] = (aux[c] = silence (block)).data();
	}
	for (size_t c = 0; c < cue.size(); ++c)
		outputs.cue[c] = (cue[c] = silence (block)).data();
	for (size_t side = 0; side < 2; ++side)
		outputs.phones[side] = phones ? (phonesPair[side] = silence (block)).data() : nullptr;

	auto& controls = deck->controls();
	const int warmUp = 400;   // ramps settle, decoders prime
	for (int i = 0; i < warmUp; ++i)
	{
		if (! standing)
			movePoses (controls, i);
		deck->process (stems.data(), outputs, block);
	}
	deck->resetTiming();

	std::vector<double> perBlock;
	perBlock.reserve ((size_t) blocks);
	for (int i = 0; i < blocks; ++i)
	{
		if (! standing)
			movePoses (controls, warmUp + i);
		const auto start = std::chrono::steady_clock::now();
		deck->process (stems.data(), outputs, block);
		perBlock.push_back (std::chrono::duration<double, std::micro> (std::chrono::steady_clock::now() - start).count());
	}

	TimingRow row;
	row.name = name;
	double sum = 0.0;
	for (auto v : perBlock)
		sum += v;
	row.totalMean = sum / (double) blocks;
	row.totalP99 = p99 (perBlock);
	row.nodes = deck->timing();
	return row;
}
}

// (0) a standing deck (R14); (a) bed + 4 encoders, nothing cued, no decoder; (b) all cued, the cue
// as B-format, no decoder (the rig); (c) (b) + the phones decoder (standalone); (1) and (1c): one
// stem playing and cued, the other three silent, without and with the phones decoder.
inline TimingTable spatialTiming (int blocks)
{
	using detail::Deck;
	return { detail::measure ("(0) standing deck", blocks, false, false, Deck::standing),
	         detail::measure ("(a) 4 stems, nothing cued", blocks, false, false),
	         detail::measure ("(b) 4 stems cued, no decoder (rig)", blocks, true, false),
	         detail::measure ("(c) (b) + phones decoder", blocks, true, true),
	         detail::measure ("(1) 1 stem cued, no decoder (rig)", blocks, true, false, Deck::playing, 1),
	         detail::measure ("(1c) (1) + phones decoder", blocks, true, true, Deck::playing, 1) };
}

inline void printTable (std::ostream& out, const TimingTable& table)
{
	char line[256];
	out << "mean / p99 us per 128-sample block at 44.1 kHz; nodes: mean us\n";
	std::snprintf (line, sizeof line, "%-36s %8s %8s | %8s %6s %7s %8s\n", "configuration", "mean", "p99",
	               "encoders", "bed", "cuebed", "phonedec");
	out << line;
	for (const auto& r : table)
	{
		std::snprintf (line, sizeof line, "%-36s %8.1f %8.1f | %8.1f %6.1f %7.1f %8.1f\n", r.name.c_str(),
		               r.totalMean, r.totalP99, r.nodes.encoders, r.nodes.bed, r.nodes.cueBed, r.nodes.phonesDecoder);
		out << line;
	}
	out.flush();
}
}
