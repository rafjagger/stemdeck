#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Key lock (a CDJ's MASTER TEMPO): the tempo changes, the pitch stays. Pure:
// the maths around the stretcher (KeyLockStretcher), testable without audio.
namespace keylock
{
	// Rubber Band's time ratio for a playback rate: how much longer the
	// output lasts than the input. Clamped, so a deck held still or a
	// runaway nudge cannot hand it 0 or infinity.
	double timeRatio (double rate);

	// What the deck's resampler does: without key lock it plays the tempo
	// (the pitch goes with it) and converts the file's rate to the device's;
	// with key lock the stretcher plays the tempo and it only converts.
	double resamplingRatio (double rate, double fileSampleRate, double deviceSampleRate, bool keyLocked);

	// Whether the stretcher is in the path: key lock on and a stretcher
	// built, and nobody scratching -- a scratch is pitch by nature.
	bool usesStretcher (bool keyLockOn, bool stretcherReady, bool scratching);

	// How Rubber Band's output lines up with its input away from rate 1,
	// measured on the stretcher (KeyLockStretcher does it once when built):
	// startOffset -- input samples, per unit of rate away from 1, that the
	// first output sample after a start lies behind it; rateLag -- input
	// samples, per unit of rate changed since the start, that what is heard
	// lies behind what was counted.
	struct Alignment
	{
		double startOffset = 0.0;
		double rateLag = 0.0;
	};

	// How much output to throw away after a start: Rubber Band's own start
	// delay, moved to where the output lines up at `rate`.
	std::int64_t startDiscard (std::size_t startDelay, double rate, const Alignment& alignment);

	// What to take off the counted playhead at `rate`, started at `startRate`.
	double lagCorrection (const Alignment& alignment, double rate, double startRate);

	// Where the ear is, in the track's samples, while the stretcher reads
	// ahead of it. Counts what went in (fed, and every loop or repeat jump
	// the reader made) against what came out (heard, in input samples: an
	// output sample at rate r is r input samples). A jump is in the
	// playhead only once the output has reached it.
	class PositionMap
	{
	public:
		void restart (std::int64_t position);
		void fed (std::int64_t samples);
		void jumped (std::int64_t to);      // the reader went on from `to`
		void heard (double inputSamples);

		double position() const;
		double latency() const { return (double) fedTotal - heardTotal; }   // read, not yet heard

	private:
		struct Segment
		{
			std::int64_t fedAt = 0;      // the input count where it starts
			std::int64_t position = 0;   // the track position there
		};

		// Enough for loops of 10 ms against a latency far beyond the
		// stretcher's; the oldest is dropped if ever full.
		std::array<Segment, 64> segments {};
		std::size_t first = 0, count = 0;
		std::int64_t fedTotal = 0;
		double heardTotal = 0.0;
	};

	// The stretcher's output as it was made: so many samples at such a
	// rate. Rubber Band applies a ratio when it processes, and hands the
	// result out a latency later; taken out, each sample stands for the
	// input at the rate it was made with.
	class OutputRuns
	{
	public:
		void clear() { first = 0; count = 0; }
		void produced (int samples, double rate);
		// The input `samples` of output stand for; beyond what was
		// recorded, at `currentRate`.
		double consume (int samples, double currentRate);

	private:
		struct Run
		{
			int samples = 0;
			double rate = 1.0;
		};

		// Far more than a latency's worth of blocks; when full, the newest
		// two are merged into one at their average rate (exact in input).
		std::array<Run, 128> runs {};
		std::size_t first = 0, count = 0;
	};
}
