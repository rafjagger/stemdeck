#include "StemHandover.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace StemHandover
{
	namespace
	{
		// Below this a stem counts as silent: the ratios of its noise floor
		// say nothing about where a phrase ends.
		constexpr double floorLevel = 1.0e-3;

		// Other before bass costs this much: it has to win by a clear rest.
		constexpr double reversedOrder = 0.75;

		// Pulls the swaps towards a third and two thirds of the overlap: decides
		// where the analysis does not, and is outweighed by any rest it finds.
		constexpr double spreadWeight = 0.3;

		// The level `direction` (+1 after, -1 before) of `t`, over half the
		// window, the nearest moments weighted most.
		double weightedLevel (const Envelope& e, double t, double windowSeconds, int direction)
		{
			const auto steps = std::max (1, (int) std::lround (windowSeconds / 2.0 / e.hop));
			double sum = 0.0, weights = 0.0;
			for (int k = 0; k < steps; ++k)
			{
				const auto weight = 1.0 - (k + 0.5) / steps;
				sum += weight * e.levelAt (t + direction * (k + 0.5) * e.hop);
				weights += weight;
			}
			return sum / weights;
		}

		double share (double part, double whole)
		{
			return (part + floorLevel) / (whole + 2.0 * floorLevel);
		}
	}

	float Envelope::levelAt (double seconds) const
	{
		if (seconds < 0.0 || hop <= 0.0)
			return 0.0f;
		const auto index = (std::size_t) (seconds / hop);
		return index < rms.size() ? rms[index] : 0.0f;
	}

	EnvelopeBuilder::EnvelopeBuilder (double sampleRate, double hop)
		: samplesPerHop ((std::size_t) std::max (1L, std::lround (sampleRate * hop)))
	{
		envelope.hop = hop;
	}

	void EnvelopeBuilder::add (const float* const* channels, int numChannels, int numSamples)
	{
		if (numChannels <= 0)
			return;
		channelsSeen = numChannels;
		for (int i = 0; i < numSamples; ++i)
		{
			for (int ch = 0; ch < numChannels; ++ch)
				squares += (double) channels[ch][i] * channels[ch][i];
			if (++filled == samplesPerHop)
			{
				envelope.rms.push_back ((float) std::sqrt (squares / (double) (filled * (std::size_t) numChannels)));
				filled = 0;
				squares = 0.0;
			}
		}
	}

	Envelope EnvelopeBuilder::finish()
	{
		if (filled > 0)
			envelope.rms.push_back ((float) std::sqrt (squares / (double) (filled * (std::size_t) channelsSeen)));
		filled = 0;
		squares = 0.0;
		return std::move (envelope);
	}

	std::vector<double> changeoverCosts (const Envelope& oldStem, const Envelope& newStem,
										 const std::vector<Downbeat>& candidates, double windowSeconds)
	{
		std::vector<double> oldAfter, costs;
		double loudest = 0.0;
		for (const auto& c : candidates)
		{
			oldAfter.push_back (weightedLevel (oldStem, c.oldSeconds, windowSeconds, 1));
			loudest = std::max (loudest, oldAfter.back());
		}

		for (std::size_t i = 0; i < candidates.size(); ++i)
		{
			const auto& c = candidates[i];
			const auto oldBefore = weightedLevel (oldStem, c.oldSeconds, windowSeconds, -1);
			const auto newBefore = weightedLevel (newStem, c.newSeconds, windowSeconds, -1);
			const auto newAfter = weightedLevel (newStem, c.newSeconds, windowSeconds, 1);

			const auto cutOff = share (oldAfter[i], oldAfter[i] + oldBefore);     // the old stem carries on past it
			const auto midNote = share (newBefore, newBefore + newAfter);         // the new one was already sounding
			const auto loud = oldAfter[i] / (loudest + floorLevel);               // the old one is loud there
			costs.push_back (cutOff + midNote + loud);
		}
		return costs;
	}

	int chooseDownbeat (const Envelope& oldStem, const Envelope& newStem,
						const std::vector<Downbeat>& candidates, double windowSeconds)
	{
		const auto costs = changeoverCosts (oldStem, newStem, candidates, windowSeconds);
		if (costs.empty())
			return -1;
		const auto best = std::min_element (costs.begin(), costs.end());
		return candidates[(std::size_t) (best - costs.begin())].bar;
	}

	Pair chooseBassAndOther (const Envelopes& oldStems, const Envelopes& newStems,
							 const std::vector<Downbeat>& downbeats, int endBar, double windowSeconds)
	{
		const auto gap = std::max (1, (int) std::lround (endBar / 5.0));

		std::vector<Downbeat> inside;
		for (const auto& d : downbeats)
			if (d.bar >= gap && d.bar <= endBar - gap)
				inside.push_back (d);

		const auto bassCosts = changeoverCosts (oldStems[bass], newStems[bass], inside, windowSeconds);
		const auto otherCosts = changeoverCosts (oldStems[other], newStems[other], inside, windowSeconds);
		const auto bassIdeal = endBar / 3.0, otherIdeal = 2.0 * endBar / 3.0;

		Pair best { endBar >= 2 ? 1 : endBar, endBar };
		auto bestCost = std::numeric_limits<double>::max();
		for (std::size_t b = 0; b < inside.size(); ++b)
			for (std::size_t o = 0; o < inside.size(); ++o)
			{
				const auto bassBar = inside[b].bar, otherBar = inside[o].bar;
				if (std::abs (bassBar - otherBar) < gap)
					continue;
				const auto spread = spreadWeight * (std::abs (bassBar - bassIdeal) + std::abs (otherBar - otherIdeal)) / endBar;
				const auto cost = bassCosts[b] + otherCosts[o] + spread + (bassBar > otherBar ? reversedOrder : 0.0);
				if (cost < bestCost)
				{
					bestCost = cost;
					best = { bassBar, otherBar };
				}
			}
		return best;
	}
}
