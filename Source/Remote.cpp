#include "Remote.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace remote
{
	namespace
	{
		constexpr int decks = 2, stemsPerDeck = 4, busCount = 6;
		constexpr int firstStemMeter = 41;

		std::vector<std::string> segments (const std::string& path)
		{
			std::vector<std::string> out;
			std::string current;
			for (const auto c : path)
			{
				if (c == '/')
				{
					out.push_back (current);
					current.clear();
				}
				else
					current += c;
			}
			out.push_back (current);
			return out;
		}

		bool isPlaceholder (const std::string& segment)
		{
			return segment.size() > 2 && segment.front() == '{' && segment.back() == '}';
		}

		std::optional<int> number (const std::string& text)
		{
			if (text.empty() || text.size() > 3)
				return std::nullopt;
			int value = 0;
			for (const auto c : text)
			{
				if (c < '0' || c > '9')
					return std::nullopt;
				value = value * 10 + (c - '0');
			}
			return value;
		}

		// The address taken apart against the pattern: each {name} as a number.
		std::optional<std::map<std::string, int>> match (const std::string& pattern, const std::string& address)
		{
			const auto want = segments (pattern), have = segments (address);
			if (want.size() != have.size())
				return std::nullopt;
			std::map<std::string, int> fields;
			for (size_t i = 0; i < want.size(); ++i)
			{
				if (! isPlaceholder (want[i]))
				{
					if (want[i] != have[i])
						return std::nullopt;
					continue;
				}
				const auto value = number (have[i]);
				if (! value)
					return std::nullopt;
				fields[want[i].substr (1, want[i].size() - 2)] = *value;
			}
			return fields;
		}

		std::string fill (const std::string& pattern, const std::map<std::string, int>& fields)
		{
			std::string out;
			for (size_t i = 0; i < pattern.size(); ++i)
			{
				const auto close = pattern[i] == '{' ? pattern.find ('}', i) : std::string::npos;
				if (close == std::string::npos)
				{
					out += pattern[i];
					continue;
				}
				const auto found = fields.find (pattern.substr (i + 1, close - i - 1));
				out += found != fields.end() ? std::to_string (found->second) : pattern.substr (i, close - i + 1);
				i = close;
			}
			return out;
		}

		bool within (int value, int low, int high) { return value >= low && value <= high; }
	}

	std::optional<Switch> parseSwitch (const Words& words, const std::string& address, int value)
	{
		const auto fields = match (words.bus, address);
		if (! fields || (value != 0 && value != 1))
			return std::nullopt;
		const auto deck = fields->at ("deck"), stem = fields->at ("stem"), bus = fields->at ("bus");
		if (! within (deck, 1, decks) || ! within (stem, 1, stemsPerDeck) || ! within (bus, 1, busCount))
			return std::nullopt;
		return Switch { deck - 1, stem - 1, bus - 1, value == 1 };
	}

	std::string reportAddress (const Words& words, int deck, int stem)
	{
		return fill (words.buses, { { "deck", deck + 1 }, { "stem", stem + 1 } });
	}

	bool isRecall (const Words& words, const std::string& address)
	{
		return address == words.recall;
	}

	std::string vuAddress (const Words& words, int deck, int stem)
	{
		return fill (words.vu, { { "n", firstStemMeter + deck * stemsPerDeck + stem } });
	}

	unsigned maskOf (const std::array<bool, 6>& on)
	{
		unsigned mask = 0;
		for (size_t bus = 0; bus < on.size(); ++bus)
			if (on[bus])
				mask |= 1u << bus;
		return mask;
	}

	float rmsOf (double sumOfSquares, long long samples)
	{
		return samples > 0 ? (float) std::sqrt (sumOfSquares / samples) : 0.0f;
	}

	std::optional<int> meterNumber (const std::vector<std::string>& meters, const std::string& name)
	{
		const auto found = std::find (meters.begin(), meters.end(), name);
		if (found == meters.end())
			return std::nullopt;
		return (int) (found - meters.begin()) + 1;
	}

	std::optional<std::string> vuAddressNamed (const Words& words, const std::string& name)
	{
		const auto n = meterNumber (words.meters, name);
		if (! n)
			return std::nullopt;
		return fill (words.vu, { { "n", *n } });
	}

	Block measure (const float* samples, int count)
	{
		Block block;
		block.samples = count;
		for (int i = 0; i < count; ++i)
		{
			block.peak = std::max (block.peak, std::abs (samples[i]));
			block.squares += (double) samples[i] * samples[i];
		}
		return block;
	}

	std::vector<Meter> levelBundle (const Words& words, const std::array<Level, stemMeters>& stems,
									const std::array<Level, 2>& aux)
	{
		static_assert (stemMeters == decks * stemsPerDeck);
		std::vector<Meter> bundle;
		bundle.reserve (stems.size() + aux.size());
		for (int deck = 0; deck < decks; ++deck)
			for (int stem = 0; stem < stemsPerDeck; ++stem)
				bundle.push_back ({ vuAddress (words, deck, stem), stems[(size_t) (deck * stemsPerDeck + stem)] });

		const std::array<const char*, 2> auxNames { "stem_aux_L", "stem_aux_R" };
		for (size_t side = 0; side < aux.size(); ++side)
			if (const auto address = vuAddressNamed (words, auxNames[side]))
				bundle.push_back ({ *address, aux[side] });
		return bundle;
	}

	MeterGate::Decision MeterGate::next (const std::vector<Level>& levels)
	{
		const auto sounding = std::any_of (levels.begin(), levels.end(),
										   [] (const Level& level) { return level.peak >= silentBelow; });
		const auto fell = wasSounding && ! sounding;
		wasSounding = sounding;
		if (sounding)
			return send;
		return fell ? sendZeros : skip;
	}

	void LevelTap::add (const Block& block)
	{
		if (block.peak > peak.load())
			peak = block.peak;
		squares = squares.load() + block.squares;
		samples = samples.load() + block.samples;
	}

	Level LevelTap::pop()
	{
		const auto p = peak.exchange (0.0f);
		const auto s = squares.exchange (0.0);
		const auto n = samples.exchange (0LL);
		return { p, rmsOf (s, n) };
	}
}
