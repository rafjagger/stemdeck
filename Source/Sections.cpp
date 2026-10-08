#include "Sections.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <initializer_list>
#include <optional>
#include <sstream>

namespace sections
{
	FeatureBuilder::FeatureBuilder (double rate, double start, double hop)
		: sampleRate (rate), startSeconds (start), hopSeconds (hop)
	{
	}

	void FeatureBuilder::ensureFrame (long long frame)
	{
		if ((long long) frameSamples.size() <= frame)
		{
			frameSquares.resize ((size_t) frame + 1, std::array<double, numStems> {});
			frameSamples.resize ((size_t) frame + 1, 0);
			frameOnsets.resize ((size_t) frame + 1, std::array<float, numStems> {});
		}
	}

	void FeatureBuilder::add (const std::array<const float*, numStems>& stems, int count)
	{
		for (int i = 0; i < count; ++i, ++sampleIndex)
		{
			const auto seconds = (double) sampleIndex / sampleRate;
			const auto slice = (long long) std::floor (seconds / sliceSeconds);

			if (slice != currentSlice)
			{
				closeSlice();
				currentSlice = slice;
			}

			const auto frame = seconds >= startSeconds ? (long long) std::floor ((seconds - startSeconds) / hopSeconds) : -1;

			if (frame >= 0)
			{
				ensureFrame (frame);
				++frameSamples[(size_t) frame];
			}

			for (size_t s = 0; s < (size_t) numStems; ++s)
			{
				const auto x = stems[s][i];
				const auto dx = x - previous[s];
				previous[s] = x;
				sliceSquares[s] += (double) dx * dx;

				if (frame >= 0)
					frameSquares[(size_t) frame][s] += (double) x * x;
			}

			++sliceSamples;
		}
	}

	void FeatureBuilder::closeSlice()
	{
		if (sliceSamples == 0)
			return;

		// By the slice's middle: the slice holding the downbeat starts before
		// it (the grid is never on a slice boundary) but its attack is the one.
		const auto sliceMiddle = ((double) currentSlice + 0.5) * sliceSeconds;
		const auto frame = sliceMiddle >= startSeconds ? (long long) std::floor ((sliceMiddle - startSeconds) / hopSeconds) : -1;

		for (size_t s = 0; s < (size_t) numStems; ++s)
		{
			const auto level = (float) std::sqrt (sliceSquares[s] / (double) sliceSamples);
			auto& before = history[s];
			const auto loudest = before.empty() ? 0.0f : *std::max_element (before.begin(), before.end());
			auto& quietFor = slicesSinceOnset[s];

			if (level > onsetFloor && level > onsetRise * loudest && quietFor >= onsetHistory && frame >= 0)
			{
				ensureFrame (frame);
				frameOnsets[(size_t) frame][s] += 1.0f;
				quietFor = 0;
			}
			else
			{
				++quietFor;
			}

			before.push_back (level);
			if ((int) before.size() > onsetHistory)
				before.erase (before.begin());

			sliceSquares[s] = 0.0;
		}

		sliceSamples = 0;
	}

	Features FeatureBuilder::finish()
	{
		closeSlice();

		Features features;
		features.startSeconds = startSeconds;
		features.hopSeconds = hopSeconds;
		features.frames.resize (frameSamples.size());

		for (size_t f = 0; f < frameSamples.size(); ++f)
			for (size_t s = 0; s < (size_t) numStems; ++s)
			{
				features.frames[f].rms[s] = frameSamples[f] > 0 ? (float) std::sqrt (frameSquares[f][s] / (double) frameSamples[f]) : 0.0f;
				features.frames[f].onsets[s] = frameOnsets[f][s];
			}

		return features;
	}

	//==========================================================================
	namespace
	{
		constexpr const char* magic = "a3-sections";
		constexpr int version = 1;
	}

	std::string encode (const Features& features)
	{
		std::ostringstream out;
		out.precision (9);
		out << magic << ' ' << version << ' ' << features.startSeconds << ' ' << features.hopSeconds << ' '
			<< features.frames.size() << '\n';

		for (const auto& frame : features.frames)
		{
			for (const auto value : frame.rms)
				out << value << ' ';
			for (size_t s = 0; s < frame.onsets.size(); ++s)
				out << frame.onsets[s] << (s + 1 < frame.onsets.size() ? ' ' : '\n');
		}

		return out.str();
	}

	std::optional<Features> decode (const std::string& text)
	{
		std::istringstream in (text);
		std::string word;
		int fileVersion = 0;
		size_t count = 0;
		Features features;

		if (! (in >> word >> fileVersion >> features.startSeconds >> features.hopSeconds >> count)
			|| word != magic || fileVersion != version || ! (features.hopSeconds > 0.0)
			|| ! std::isfinite (features.hopSeconds) || ! std::isfinite (features.startSeconds))
			return std::nullopt;

		features.frames.resize (count);

		for (auto& frame : features.frames)
		{
			for (auto& value : frame.rms)
				if (! (in >> value))
					return std::nullopt;
			for (auto& value : frame.onsets)
				if (! (in >> value))
					return std::nullopt;
		}

		return features;
	}

	//==========================================================================
	namespace
	{
		bool contains (std::string name, const char* word)
		{
			std::transform (name.begin(), name.end(), name.begin(), [] (unsigned char c) { return (char) std::tolower (c); });
			return name.find (word) != std::string::npos;
		}
	}

	StemRoles rolesFor (const std::array<std::string, numStems>& stemNames)
	{
		std::optional<int> drums, bass;

		for (int s = 0; s < numStems; ++s)
		{
			const auto& name = stemNames[(size_t) s];
			if (! drums && (contains (name, "drum") || contains (name, "kick")))
				drums = s;
			else if (! bass && contains (name, "bass"))
				bass = s;
		}

		// A role the names do not give goes by the stem creator's order,
		// skipping the stem the other role took.
		const auto firstFree = [] (std::initializer_list<int> order, int taken) {
			for (const auto s : order)
				if (s != taken)
					return s;
			return 0;
		};
		StemRoles roles;
		roles.drums = drums ? *drums : firstFree ({ 0, 1, 2, 3 }, bass.value_or (-1));
		roles.bass = bass ? *bass : firstFree ({ 1, 0, 2, 3 }, roles.drums);
		return roles;
	}

	std::vector<BarLevels> barLevels (const Features& features, double bpm, double firstBeat, StemRoles roles)
	{
		std::vector<BarLevels> bars;

		if (! (bpm > 0.0) || ! (features.hopSeconds > 0.0) || ! std::isfinite (bpm) || ! std::isfinite (firstBeat)
			|| ! std::isfinite (features.hopSeconds) || ! std::isfinite (features.startSeconds))
			return bars;

		const auto beatSeconds = 60.0 / bpm;
		const auto barSeconds = beatSeconds * beatsPerBar;
		std::vector<int> framesInBar;

		for (size_t f = 0; f < features.frames.size(); ++f)
		{
			const auto middle = features.startSeconds + ((double) f + 0.5) * features.hopSeconds;

			if (middle < firstBeat)
				continue;

			const auto bar = (size_t) std::floor ((middle - firstBeat) / barSeconds);

			if (bars.size() <= bar)
			{
				bars.resize (bar + 1);
				framesInBar.resize (bar + 1, 0);
			}

			const auto& frame = features.frames[f];
			auto& levels = bars[bar];
			levels.drums += frame.rms[(size_t) roles.drums];
			levels.bass += frame.rms[(size_t) roles.bass];

			float squares = 0.0f;
			for (const auto value : frame.rms)
				squares += value * value;
			levels.total += std::sqrt (squares);

			levels.drumOnsetsPerBeat += frame.onsets[(size_t) roles.drums];
			++framesInBar[bar];
		}

		for (size_t b = 0; b < bars.size(); ++b)
		{
			const auto n = (float) std::max (1, framesInBar[b]);
			const auto coveredBeats = (float) (framesInBar[b] * features.hopSeconds / beatSeconds);
			bars[b].drums /= n;
			bars[b].bass /= n;
			bars[b].total /= n;
			bars[b].drumOnsetsPerBeat = coveredBeats > 0.0f ? bars[b].drumOnsetsPerBeat / coveredBeats : 0.0f;
		}

		return bars;
	}

	//==========================================================================
	namespace
	{
		enum class Level { quiet, partial, full };

		float reference (std::vector<float> values)
		{
			if (values.empty())
				return 0.0f;
			const auto index = (size_t) std::floor (referencePercentile * (float) (values.size() - 1));
			std::nth_element (values.begin(), values.begin() + (long) index, values.end());
			return values[index];
		}

		template <typename Member>
		float referenceOf (const std::vector<BarLevels>& levels, Member member)
		{
			std::vector<float> values;
			for (const auto& bar : levels)
				values.push_back (bar.*member);
			return reference (values);
		}

		// Stretches shorter than minRunBars take the class of the one before.
		void smooth (std::vector<Level>& raw)
		{
			size_t start = 0;
			while (start < raw.size())
			{
				auto end = start;
				while (end < raw.size() && raw[end] == raw[start])
					++end;
				if (start > 0 && (int) (end - start) < minRunBars)
					std::fill (raw.begin() + (long) start, raw.begin() + (long) end, raw[start - 1]);
				start = end;
			}
		}

		float meanOnsets (const std::vector<BarLevels>& levels, size_t from, size_t to)
		{
			float sum = 0.0f;
			for (auto b = from; b < to; ++b)
				sum += levels[b].drumOnsetsPerBeat;
			return to > from ? sum / (float) (to - from) : 0.0f;
		}

		// The longest phrase of `partialStart`..`fullStart` that rises: its
		// first bar, or none.
		std::optional<size_t> buildBefore (const std::vector<BarLevels>& levels, size_t partialStart, size_t fullStart)
		{
			for (const auto phrase : buildPhrases)
			{
				if (fullStart < partialStart + (size_t) phrase)
					continue;
				const auto from = fullStart - (size_t) phrase;
				const auto middle = from + (size_t) phrase / 2;
				const auto first = meanOnsets (levels, from, middle);
				const auto second = meanOnsets (levels, middle, fullStart);
				if (second >= buildMinOnsetsPerBeat && second >= buildRise * first)
					return from;
			}
			return std::nullopt;
		}
	}

	std::vector<Bar> classify (const std::vector<BarLevels>& levels)
	{
		std::vector<Bar> bars (levels.size());

		const auto drumsRef = referenceOf (levels, &BarLevels::drums);
		const auto bassRef = referenceOf (levels, &BarLevels::bass);
		const auto totalRef = referenceOf (levels, &BarLevels::total);

		for (size_t b = 0; b < levels.size(); ++b)
			bars[b].energy = totalRef > 0.0f ? std::clamp (levels[b].total / totalRef, 0.0f, 1.0f) : 0.0f;

		// Without drums there is nothing to tell sections by: all groove.
		if (drumsRef < silentStemRms)
			return bars;

		const bool bassPlays = bassRef >= silentStemRms;
		std::vector<Level> raw (levels.size());

		for (size_t b = 0; b < levels.size(); ++b)
		{
			const auto drums = levels[b].drums / drumsRef;
			const auto bassFull = ! bassPlays || levels[b].bass / bassRef >= fullFrom;
			raw[b] = drums < drumsGoneBelow ? Level::quiet
				   : drums >= fullFrom && bassFull ? Level::full
				   : Level::partial;
		}

		smooth (raw);

		for (size_t b = 0; b < raw.size(); ++b)
			bars[b].section = raw[b] == Level::quiet ? Section::breakdown : Section::groove;

		for (size_t fullStart = 1; fullStart < raw.size(); ++fullStart)
		{
			if (raw[fullStart] != Level::full || raw[fullStart - 1] == Level::full)
				continue;

			auto partialStart = fullStart;
			while (partialStart > 0 && raw[partialStart - 1] == Level::partial)
				--partialStart;
			auto quietStart = partialStart;
			while (quietStart > 0 && raw[quietStart - 1] == Level::quiet)
				--quietStart;

			const auto build = buildBefore (levels, partialStart, fullStart);
			if (build)
				for (auto b = *build; b < fullStart; ++b)
					bars[b].section = Section::build;

			const auto breakdownBars = (int) (partialStart - quietStart);
			if (! build && breakdownBars < breakdownBeforeDropBars)
				continue;

			for (auto b = fullStart; b < raw.size() && b < fullStart + (size_t) dropLastsBars && raw[b] == Level::full; ++b)
				bars[b].section = Section::drop;
		}

		return bars;
	}

	const char* nameOf (Section section)
	{
		switch (section)
		{
			case Section::groove:    return "groove";
			case Section::build:     return "build";
			case Section::drop:      return "drop";
			case Section::breakdown: return "breakdown";
		}
		return "groove";
	}
}
