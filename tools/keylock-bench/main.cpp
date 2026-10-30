// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
// KEY quality offline: one deck's four stereo stems through each key lock
// engine (KeyLockStretcher) at rates 0.94, 1.06 and a ramp 0.94 -> 1.06,
// timed. No JACK, no audio device, nothing played.
//   stemdeck-keylock-bench [seconds] [input.wav]   (default 20 s, a synthetic mix)
// With KEYLOCK_BENCH_WAV_DIR set, the four stems' sum of every run is written
// there as <engine>-<case>.wav (32-bit float), to compare by ear.
#include "KeyLockStretcher.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{
constexpr int numStems = 4;
constexpr int block = 256;   // a JACK period's worth
constexpr double pi = 3.14159265358979323846;

using Stereo = std::vector<std::vector<float>>;   // [channel][frame]

// Four stems of a 124 BPM loop: kick and hats, a bass, a chord pad, a lead
// with vibrato -- transients, low end, sustained tones and a moving pitch,
// each where a stretcher shows its faults.
std::vector<Stereo> syntheticStems (double rate, double seconds)
{
	const auto length = (size_t) (seconds * rate);
	std::vector<Stereo> stems (numStems, Stereo (2, std::vector<float> (length)));
	const double beat = 60.0 / 124.0;
	std::uint32_t noise = 1;

	for (size_t i = 0; i < length; ++i)
	{
		const double t = (double) i / rate;
		const double inBeat = std::fmod (t, beat), inEighth = std::fmod (t, beat / 2.0);
		noise = noise * 1664525u + 1013904223u;
		const double white = (double) (noise >> 8) / (double) (1u << 24) * 2.0 - 1.0;

		const double kick = std::sin (2.0 * pi * (50.0 * inBeat + 60.0 * (1.0 - std::exp (-inBeat * 30.0)) / 30.0)) * std::exp (-inBeat * 9.0);
		const double hat = inEighth > beat / 4.0 ? white * std::exp (-(inEighth - beat / 4.0) * 60.0) : 0.0;
		const double bassHz = std::fmod (t, 4.0 * beat) < 2.0 * beat ? 55.0 : 73.42;
		const double bass = std::tanh (3.0 * std::sin (2.0 * pi * bassHz * t));
		double pad = 0.0;
		for (const double hz : { 220.0, 277.18, 329.63, 440.0 })
			pad += std::sin (2.0 * pi * hz * t + std::sin (2.0 * pi * 0.3 * t));
		const double lead = std::sin (2.0 * pi * 587.33 * t + 3.0 * std::sin (2.0 * pi * 5.5 * t));

		// Their sum stays below full scale, so the written mix does not clip.
		const double left[] = { 0.35 * kick + 0.12 * hat, 0.18 * bass, 0.06 * pad, 0.1 * lead };
		const double right[] = { 0.35 * kick + 0.08 * hat, 0.18 * bass, 0.06 * std::cos (pad), 0.08 * lead };
		for (int s = 0; s < numStems; ++s)
		{
			stems[(size_t) s][0][i] = (float) left[s];
			stems[(size_t) s][1][i] = (float) right[s];
		}
	}
	return stems;
}

// A WAV file's frames as stereo float: 16/24/32-bit PCM or 32-bit float,
// mono doubled. Empty when it cannot read it.
Stereo readWav (const std::string& path, double& rate)
{
	std::ifstream in (path, std::ios::binary);
	std::vector<char> bytes ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char>());
	auto u16 = [&] (size_t at) { std::uint16_t v; std::memcpy (&v, &bytes[at], 2); return v; };
	auto u32 = [&] (size_t at) { std::uint32_t v; std::memcpy (&v, &bytes[at], 4); return v; };

	if (bytes.size() < 12 || std::memcmp (bytes.data(), "RIFF", 4) != 0 || std::memcmp (&bytes[8], "WAVE", 4) != 0)
		return {};

	int format = 0, channels = 0, bits = 0;
	for (size_t at = 12; at + 8 <= bytes.size();)
	{
		const auto size = (size_t) u32 (at + 4);
		const auto body = at + 8;
		if (body + size > bytes.size())
			break;
		if (std::memcmp (&bytes[at], "fmt ", 4) == 0)
		{
			format = u16 (body);
			channels = u16 (body + 2);
			rate = (double) u32 (body + 4);
			bits = u16 (body + 14);
			if (format == 0xFFFE && size >= 26)
				format = u16 (body + 24);   // WAVE_FORMAT_EXTENSIBLE: the subformat's tag
		}
		else if (std::memcmp (&bytes[at], "data", 4) == 0 && channels > 0)
		{
			const auto width = (size_t) bits / 8;
			const auto frames = size / (width * (size_t) channels);
			Stereo out (2, std::vector<float> (frames));
			for (size_t f = 0; f < frames; ++f)
				for (int ch = 0; ch < 2; ++ch)
				{
					const auto p = body + (f * (size_t) channels + (size_t) std::min (ch, channels - 1)) * width;
					float v = 0.0f;
					if (format == 3 && bits == 32)
						std::memcpy (&v, &bytes[p], 4);
					else if (format == 1 && bits == 16)
						v = (float) (std::int16_t) u16 (p) / 32768.0f;
					else if (format == 1 && bits == 24)
						v = (float) ((std::int32_t) ((std::uint32_t) (std::uint8_t) bytes[p] << 8 | (std::uint32_t) (std::uint8_t) bytes[p + 1] << 16
													 | (std::uint32_t) (std::uint8_t) bytes[p + 2] << 24) >> 8) / 8388608.0f;
					else if (format == 1 && bits == 32)
						v = (float) (std::int32_t) u32 (p) / 2147483648.0f;
					else
						return {};
					out[(size_t) ch][f] = v;
				}
			return out;
		}
		at = body + size + (size & 1);
	}
	return {};
}

void writeWav (const std::string& path, const Stereo& audio, double rate)
{
	const auto frames = (std::uint32_t) audio[0].size();
	const std::uint32_t dataBytes = frames * 2 * 4;
	std::ofstream out (path, std::ios::binary);
	auto put32 = [&] (std::uint32_t v) { out.write (reinterpret_cast<const char*> (&v), 4); };
	auto put16 = [&] (std::uint16_t v) { out.write (reinterpret_cast<const char*> (&v), 2); };
	out.write ("RIFF", 4); put32 (36 + dataBytes); out.write ("WAVE", 4);
	out.write ("fmt ", 4); put32 (16); put16 (3); put16 (2); put32 ((std::uint32_t) rate);
	put32 ((std::uint32_t) rate * 8); put16 (8); put16 (32);
	out.write ("data", 4); put32 (dataBytes);
	for (std::uint32_t f = 0; f < frames; ++f)
		for (int ch = 0; ch < 2; ++ch)
			out.write (reinterpret_cast<const char*> (&audio[(size_t) ch][f]), 4);
}

// A stem, read as the deck reads it; looped, so any length of output has input.
struct StemInput : KeyLockStretcher::Input
{
	explicit StemInput (const Stereo& s) : stem (s) {}

	int read (float* const* dest, int count, std::int64_t position) override
	{
		const auto length = (std::int64_t) stem[0].size();
		const auto n = (int) std::clamp<std::int64_t> (length - position, 0, count);
		for (int ch = 0; ch < 2; ++ch)
			std::copy_n (stem[(size_t) ch].begin() + position, n, dest[ch]);
		return n;
	}

	std::int64_t jumpFrom (std::int64_t) override { return 0; }

	const Stereo& stem;
};

struct Case
{
	const char* name;
	double from, to;   // the rate at the start and at the end: a ramp when they differ
};

struct Result
{
	double audioSeconds = 0.0, cpuSeconds = 0.0;
	double realtimeFactor() const { return audioSeconds / cpuSeconds; }
};

Result run (keylock::Engine engine, const Case& c, const std::vector<Stereo>& stems, double rate, double seconds, Stereo* mix)
{
	std::vector<std::unique_ptr<KeyLockStretcher>> stretchers;
	std::vector<StemInput> inputs;
	const auto alignment = KeyLockStretcher::alignmentFor (rate, engine);
	for (int s = 0; s < numStems; ++s)
	{
		stretchers.push_back (std::make_unique<KeyLockStretcher> (2, rate, engine, alignment));
		inputs.emplace_back (stems[(size_t) s]);
	}
	for (int s = 0; s < numStems; ++s)
		stretchers[(size_t) s]->restart (0, c.from, inputs[(size_t) s]);

	const auto blocks = (int) (seconds * rate / block);
	Stereo out (2, std::vector<float> (block));
	float* pointers[] = { out[0].data(), out[1].data() };
	if (mix != nullptr)
		mix->assign (2, std::vector<float> ((size_t) blocks * block, 0.0f));

	std::chrono::steady_clock::duration busy {};
	for (int b = 0; b < blocks; ++b)
	{
		const auto playbackRate = c.from + (c.to - c.from) * b / std::max (1, blocks - 1);
		for (int s = 0; s < numStems; ++s)
		{
			const auto start = std::chrono::steady_clock::now();
			stretchers[(size_t) s]->render (pointers, block, playbackRate, inputs[(size_t) s]);
			busy += std::chrono::steady_clock::now() - start;

			if (mix != nullptr)
				for (int ch = 0; ch < 2; ++ch)
					for (int i = 0; i < block; ++i)
						(*mix)[(size_t) ch][(size_t) (b * block + i)] += out[(size_t) ch][(size_t) i];
		}
	}
	return { (double) blocks * block / rate, std::chrono::duration<double> (busy).count() };
}
}

int main (int argc, char** argv)
{
	const double seconds = argc > 1 ? std::atof (argv[1]) : 20.0;
	if (! (seconds >= 1.0))
	{
		std::cerr << "usage: stemdeck-keylock-bench [seconds >= 1] [input.wav]\n";
		return 2;
	}

	double rate = 44100.0;
	std::vector<Stereo> stems;
	if (argc > 2)
	{
		auto file = readWav (argv[2], rate);
		if (file.empty() || file[0].empty())
		{
			std::cerr << "cannot read " << argv[2] << " (16/24/32-bit PCM or 32-bit float WAV)\n";
			return 2;
		}
		// The whole file on every stem: four times the work of one, as a deck has.
		for (auto& ch : file)
			for (auto& v : ch)
				v *= 1.0f / numStems;
		stems.assign (numStems, file);
	}
	else
	{
		stems = syntheticStems (rate, seconds * 1.1 + 1.0);
	}

	const char* wavDir = std::getenv ("KEYLOCK_BENCH_WAV_DIR");
	const Case cases[] = { { "0.94", 0.94, 0.94 }, { "1.06", 1.06, 1.06 }, { "ramp", 0.94, 1.06 } };
	const keylock::Engine engines[] = { keylock::Engine::standard, keylock::Engine::high };

	std::cout << "key lock bench: " << numStems << " stereo stems, " << seconds << " s at " << rate << " Hz, blocks of " << block << "\n"
			  << std::left << std::setw (10) << "engine" << std::setw (8) << "case"
			  << std::right << std::setw (14) << "realtime x" << std::setw (16) << "% of one core" << "\n";

	for (const auto engine : engines)
	{
		KeyLockStretcher::alignmentFor (rate, engine);   // measured before the clock runs
		for (const auto& c : cases)
		{
			Stereo mix;
			const auto result = run (engine, c, stems, rate, seconds, wavDir != nullptr ? &mix : nullptr);
			std::cout << std::left << std::setw (10) << keylock::engineName (engine) << std::setw (8) << c.name
					  << std::right << std::fixed << std::setprecision (1) << std::setw (14) << result.realtimeFactor()
					  << std::setw (16) << 100.0 / result.realtimeFactor() << "\n";
			if (wavDir != nullptr)
				writeWav (std::string (wavDir) + "/" + keylock::engineName (engine) + "-" + c.name + ".wav", mix, rate);
		}
	}
	return 0;
}
