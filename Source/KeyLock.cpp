#include "KeyLock.h"

#include <algorithm>
#include <cmath>

namespace keylock
{
	namespace
	{
		// Far outside what a fader, a jog and a sync nudge reach together.
		constexpr double slowest = 0.05, fastest = 8.0;
	}

	double timeRatio (double rate)
	{
		return 1.0 / std::clamp (rate, slowest, fastest);
	}

	double resamplingRatio (double rate, double fileSampleRate, double deviceSampleRate, bool keyLocked)
	{
		return (keyLocked ? 1.0 : rate) * fileSampleRate / deviceSampleRate;
	}

	bool usesStretcher (bool keyLockOn, bool stretcherReady, bool scratching)
	{
		return keyLockOn && stretcherReady && ! scratching;
	}

	std::int64_t startDiscard (std::size_t startDelay, double rate, const Alignment& alignment)
	{
		const auto r = std::clamp (rate, slowest, fastest);
		const auto discard = (double) startDelay + alignment.startOffset * (r - 1.0) / r;
		return std::max<std::int64_t> (0, std::llround (discard));
	}

	double lagCorrection (const Alignment& alignment, double rate, double startRate)
	{
		return alignment.rateLag * (rate - startRate);
	}

	//==============================================================================
	void PositionMap::restart (std::int64_t position)
	{
		first = 0;
		count = 1;
		segments[0] = { 0, position };
		fedTotal = 0;
		heardTotal = 0.0;
	}

	void PositionMap::fed (std::int64_t samples)
	{
		fedTotal += samples;
	}

	void PositionMap::jumped (std::int64_t to)
	{
		if (count == segments.size())
		{
			first = (first + 1) % segments.size();
			--count;
		}

		segments[(first + count) % segments.size()] = { fedTotal, to };
		++count;
	}

	void PositionMap::heard (double inputSamples)
	{
		heardTotal += inputSamples;

		// Jumps the output has passed are history.
		while (count > 1 && (double) segments[(first + 1) % segments.size()].fedAt <= heardTotal)
		{
			first = (first + 1) % segments.size();
			--count;
		}
	}

	double PositionMap::position() const
	{
		if (count == 0)
			return 0.0;

		const auto& segment = segments[first];
		return (double) segment.position + (heardTotal - (double) segment.fedAt);
	}

	//==============================================================================
	void OutputRuns::produced (int samples, double rate)
	{
		if (samples <= 0)
			return;

		if (count > 0)
		{
			auto& last = runs[(first + count - 1) % runs.size()];

			// Same rate (a fader left alone): one run; full: merged, exact in input.
			if (std::abs (last.rate - rate) < 1e-12 || count == runs.size())
			{
				const auto total = last.samples + samples;
				last.rate = (last.samples * last.rate + samples * rate) / total;
				last.samples = total;
				return;
			}
		}

		runs[(first + count) % runs.size()] = { samples, rate };
		++count;
	}

	double OutputRuns::consume (int samples, double currentRate)
	{
		double input = 0.0;

		while (samples > 0 && count > 0)
		{
			auto& run = runs[first];
			const auto take = std::min (samples, run.samples);
			input += take * run.rate;
			run.samples -= take;
			samples -= take;

			if (run.samples == 0)
			{
				first = (first + 1) % runs.size();
				--count;
			}
		}

		return input + samples * currentRate;
	}
}
