#pragma once

#include "Sections.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

// Four stems made from a bar script, one letter per bar, for the section
// tests. 120 BPM (a beat 0.5 s, a bar 2 s), the downbeat at 0.25 s,
// 8000 samples a second. Every bar has a quiet pad on "other"; on top:
//   F  full: kick on every beat, a hat on every off-beat (drums), bass
//   k  kick and hats, no bass
//   Q  drums and bass gone
//   q  a snare on every beat, no bass
//   s  a snare on every sixteenth, no bass
namespace synthetic
{
	constexpr double sampleRate = 8000.0;
	constexpr double bpm = 120.0;
	constexpr double beat = 0.5;
	constexpr double firstBeat = 0.25;
	constexpr double pi = 3.14159265358979323846;

	struct Stems
	{
		std::array<std::vector<float>, sections::numStems> samples;
	};

	inline float noise (uint32_t& state)
	{
		state = state * 1664525u + 1013904223u;
		return (float) ((double) (state >> 8) / (double) (1u << 24) * 2.0 - 1.0);
	}

	// A hit at `at` seconds: a sine (hz > 0) or noise (hz == 0), decaying.
	inline void hit (std::vector<float>& into, double at, double hz, float level, double decay, double length, uint32_t& state)
	{
		const auto start = (long) std::lround (at * sampleRate);
		const auto count = (long) std::lround (length * sampleRate);
		for (long i = 0; i < count && start + i < (long) into.size(); ++i)
		{
			const auto t = (double) i / sampleRate;
			const auto tone = hz > 0.0 ? (float) std::sin (2.0 * pi * hz * t) : noise (state);
			into[(size_t) (start + i)] += level * (float) std::exp (-t / decay) * tone;
		}
	}

	inline Stems fromScript (const std::string& bars)
	{
		Stems stems;
		const auto barSeconds = 4 * beat;
		const auto total = (size_t) ((firstBeat + barSeconds * (double) bars.size()) * sampleRate);
		for (auto& stem : stems.samples)
			stem.assign (total, 0.0f);
		auto& drums = stems.samples[0];
		auto& bass = stems.samples[1];
		auto& other = stems.samples[2];
		uint32_t state = 1;

		for (size_t i = 0; i < total; ++i)
			other[i] = 0.05f * (float) std::sin (2.0 * pi * 220.0 * (double) i / sampleRate);

		for (size_t b = 0; b < bars.size(); ++b)
		{
			const auto letter = bars[b];
			const auto barStart = firstBeat + (double) b * barSeconds;

			for (int n = 0; n < 4; ++n)
			{
				const auto at = barStart + n * beat;
				if (letter == 'F' || letter == 'k')
				{
					hit (drums, at, 60.0, 0.8f, 0.08, 0.15, state);
					hit (drums, at + beat / 2, 0.0, 0.3f, 0.01, 0.03, state);
				}
				if (letter == 'q')
					hit (drums, at, 0.0, 0.5f, 0.03, 0.08, state);
				if (letter == 's')
					for (int sixteenth = 0; sixteenth < 4; ++sixteenth)
						hit (drums, at + sixteenth * beat / 4, 0.0, 0.5f, 0.03, 0.08, state);
			}

			if (letter == 'F')
				for (auto i = (size_t) (barStart * sampleRate); i < (size_t) ((barStart + barSeconds) * sampleRate) && i < total; ++i)
					bass[i] = 0.5f * (float) std::sin (2.0 * pi * 55.0 * (double) i / sampleRate);
		}

		return stems;
	}

	// Features as StemDeck measures them: one frame per beat from the downbeat.
	inline sections::Features featuresOf (const Stems& stems, int blockSize = 4096)
	{
		sections::FeatureBuilder builder (sampleRate, firstBeat, beat);
		const auto total = stems.samples[0].size();
		for (size_t start = 0; start < total; start += (size_t) blockSize)
		{
			const auto count = (int) std::min ((size_t) blockSize, total - start);
			builder.add ({ stems.samples[0].data() + start, stems.samples[1].data() + start,
						   stems.samples[2].data() + start, stems.samples[3].data() + start }, count);
		}
		return builder.finish();
	}
}
