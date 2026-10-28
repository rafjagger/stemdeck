#include "Remote.h"

#include "Buses.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace remote
{
	namespace
	{
		constexpr int decks = 2, stemsPerDeck = 4;
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
		if (! within (deck, 1, decks) || ! within (stem, 1, stemsPerDeck) || ! within (bus, 1, buses::count))
			return std::nullopt;
		if (! buses::mayUse (buses::stemIndex (deck - 1, stem - 1), bus - 1))
			return std::nullopt;   // stem n plays only on channel n or AUX
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

	std::optional<float> parseDeckThreeD (const Words& words, const std::string& address, int oscDeck, float value)
	{
		if (words.deckThreeD.empty() || std::isnan (value))
			return std::nullopt;
		const auto fields = match (words.deckThreeD, address);
		const auto deck = fields ? fields->find ("deck") : std::map<std::string, int>::const_iterator();
		if (! fields || deck == fields->end() || deck->second != oscDeck)
			return std::nullopt;
		return std::clamp (value, 0.0f, 1.0f);
	}

	int deckListenPort (int stemdeckOscPort, int oscDeck)
	{
		if (stemdeckOscPort <= 0 || oscDeck < 1)
			return -1;
		return stemdeckOscPort + oscDeck - 1;
	}

	std::string deckHelloAddress (const Words& words, int oscDeck)
	{
		return words.deckHello.empty() ? std::string() : fill (words.deckHello, { { "deck", oscDeck } });
	}

	std::optional<TempoSend> tempoMessage (const Words& words, int oscDeck, const Tempo& tempo)
	{
		if (words.deckTempo.empty())
			return std::nullopt;
		return TempoSend { fill (words.deckTempo, { { "deck", oscDeck } }), (float) tempo.bpm,
						   tempo.playing ? 1 : 0, tempo.master ? 1 : 0 };
	}

	double tempoOf (bool loaded, double gridBpm, double rate)
	{
		return loaded && gridBpm > 0.0 ? gridBpm * rate : 0.0;
	}

	std::optional<Tempo> TempoReport::next (const Tempo& now, double nowMs)
	{
		if (! sent)
		{
			sent = now;
			sentAtMs = nowMs;
			return now;
		}
		const auto gap = nowMs - sentAtMs;
		if (gap < minGapMs)
			return std::nullopt;
		// The epsilon keeps a step of exactly 0.01 out, which doubles would
		// otherwise put just above it.
		const auto changed = now.playing != sent->playing || now.master != sent->master
							 || std::abs (now.bpm - sent->bpm) > bpmDeadband + 1e-9;
		if (! changed && gap < repeatMs)
			return std::nullopt;
		sent = now;
		sentAtMs = nowMs;
		return now;
	}

	std::vector<Outgoing> oneDeckSends (const Words& words, int oscDeck, const std::string& host)
	{
		const auto hello = deckHelloAddress (words, oscDeck);
		if (hello.empty())
			return {};
		return { { hello, host } };
	}

	unsigned maskOf (const std::array<bool, buses::count>& on)
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

	// Both are linear gains, so scaling the measurement is exact for the stem
	// alone: peak by the gain, the squares by its square.
	Block sentToBus (const Block& afterKnob, float fader)
	{
		const auto gain = buses::sendGain (fader);
		return { afterKnob.peak * gain, afterKnob.squares * gain * gain, afterKnob.samples };
	}

	Block beforeFader (const Block& afterKnob)
	{
		return sentToBus (afterKnob, 1.0f);
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
	std::vector<int> BusReports::start (const buses::Masks& now)
	{
		up = true;
		return everything (now);
	}

	std::vector<int> BusReports::changed (const buses::Masks& now, int answered)
	{
		std::vector<int> stems;
		if (! up)
			return stems;
		for (int stem = 0; stem < buses::stemCount; ++stem)
			if (stem == answered || told[(size_t) stem] != now[(size_t) stem])
				stems.push_back (stem);
		told = now;
		return stems;
	}

	std::vector<int> BusReports::everything (const buses::Masks& now)
	{
		if (! up)
			return {};
		told = now;
		std::vector<int> stems;
		for (int stem = 0; stem < buses::stemCount; ++stem)
			stems.push_back (stem);
		return stems;
	}
}
