#include "BarPhase.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace
{
	constexpr double pi = 3.14159265358979323846;

	// Magnitude spectra of Hann-windowed frames: a plain radix-2 FFT with its
	// tables made once per size.
	class Spectra
	{
	public:
		explicit Spectra (int frameSize) : size ((size_t) frameSize), window (size), cosines (size / 2), sines (size / 2),
										   reversed (size), re (size), im (size)
		{
			for (size_t i = 0; i < size; ++i)
				window[i] = (float) (0.5 - 0.5 * std::cos (2.0 * pi * (double) i / (double) size));

			for (size_t i = 0; i < size / 2; ++i)
			{
				cosines[i] = (float) std::cos (-2.0 * pi * (double) i / (double) size);
				sines[i] = (float) std::sin (-2.0 * pi * (double) i / (double) size);
			}

			for (size_t i = 1, j = 0; i < size; ++i)
			{
				auto bit = size >> 1;
				for (; (j & bit) != 0; bit >>= 1)
					j ^= bit;
				j ^= bit;
				reversed[i] = j;
			}
		}

		// |X(k)| for k = 0 .. size / 2 of the `size` samples from `input`.
		void magnitudes (const float* input, std::vector<float>& out)
		{
			for (size_t i = 0; i < size; ++i)
			{
				re[reversed[i]] = input[i] * window[i];
				im[reversed[i]] = 0.0f;
			}

			for (size_t length = 2; length <= size; length <<= 1)
			{
				const auto half = length / 2;
				const auto stride = size / length;

				for (size_t start = 0; start < size; start += length)
					for (size_t k = 0; k < half; ++k)
					{
						const auto c = cosines[k * stride], s = sines[k * stride];
						const auto a = start + k, b = a + half;
						const auto vr = re[b] * c - im[b] * s;
						const auto vi = re[b] * s + im[b] * c;
						re[b] = re[a] - vr;
						im[b] = im[a] - vi;
						re[a] += vr;
						im[a] += vi;
					}
			}

			out.resize (size / 2 + 1);
			for (size_t k = 0; k < out.size(); ++k)
				out[k] = std::sqrt (re[k] * re[k] + im[k] * im[k]);
		}

	private:
		size_t size;
		std::vector<float> window, cosines, sines;
		std::vector<size_t> reversed;
		std::vector<float> re, im;
	};

	template <typename PerFrame>
	bool forEachFrame (const std::vector<float>& signal, int size, int hop, const std::function<bool()>& shouldAbort,
					   PerFrame&& perFrame)
	{
		Spectra spectra (size);
		std::vector<float> magnitude;

		for (int t = 0; (size_t) (t * hop + size) <= signal.size(); ++t)
		{
			if ((t & 511) == 0 && shouldAbort())
				return false;

			spectra.magnitudes (signal.data() + t * hop, magnitude);
			perFrame (t, magnitude);
		}

		return true;
	}

	//==========================================================================
	// What a stem does around each beat.
	constexpr int frameSize = 1024;      // ~93 ms at 11 kHz
	constexpr int frameHop = 256;        // ~23 ms
	constexpr int chromaSize = 4096;     // fine enough to tell bass notes apart
	constexpr int chromaHop = 1024;
	constexpr double lowTop = 150.0;     // kick and bass fundamentals
	constexpr double highBottom = 3000.0;// hats, crashes, rides
	constexpr double onsetReach = 0.05;  // an onset counts for a beat this close to it (s)

	enum Band { low, mid, high, numBands };

	struct Grid
	{
		double first = 0.0;   // beat 0: the earliest beat at or after 0 s
		double length = 0.0;
		int count = 0;

		double at (int k) const { return first + k * length; }
	};

	using Chroma = std::array<double, 12>;

	// A stem frame by frame, measured once and then laid onto any grid.
	struct Frames
	{
		double rate = 0.0;   // frames per second
		double firstCentre = 0.0;
		std::array<std::vector<float>, numBands> flux, power;
		double chromaRate = 0.0, chromaFirstCentre = 0.0;
		std::vector<Chroma> chroma;
	};

	// Per beat of a grid: the sharpest onset near it, and the mean power over it, per band.
	struct StemFeatures
	{
		bool present = false;
		std::array<std::vector<double>, numBands> onset, power;
		std::vector<Chroma> chroma;
	};

	Band bandOf (double frequency)
	{
		return frequency < lowTop ? low : frequency < highBottom ? mid : high;
	}

	bool measureFrames (const std::vector<float>& signal, double rate, bool withChroma, Frames& out,
						const std::function<bool()>& shouldAbort)
	{
		out.rate = rate / frameHop;
		out.firstCentre = frameSize / 2.0 / rate;
		std::vector<float> previous ((size_t) frameSize / 2 + 1, 0.0f);
		const auto binHz = rate / frameSize;

		const auto ok = forEachFrame (signal, frameSize, frameHop, shouldAbort, [&] (int t, const std::vector<float>& magnitude)
		{
			std::array<float, numBands> flux {}, power {};

			for (size_t k = 1; k < magnitude.size(); ++k)
			{
				const auto band = bandOf ((double) k * binHz);
				const auto compressed = std::log1p (100.0f * magnitude[k]);
				flux[band] += std::max (0.0f, compressed - previous[k]);
				power[band] += magnitude[k] * magnitude[k];
				previous[k] = compressed;
			}

			for (int b = 0; b < numBands; ++b)
			{
				out.flux[(size_t) b].push_back (t == 0 ? 0.0f : flux[(size_t) b]);
				out.power[(size_t) b].push_back (power[(size_t) b]);
			}
		});

		if (! ok || ! withChroma)
			return ok;

		out.chromaRate = rate / chromaHop;
		out.chromaFirstCentre = chromaSize / 2.0 / rate;
		const auto chromaBinHz = rate / chromaSize;

		return forEachFrame (signal, chromaSize, chromaHop, shouldAbort, [&] (int, const std::vector<float>& magnitude)
		{
			Chroma chroma {};
			for (size_t k = 1; k < magnitude.size(); ++k)
			{
				const auto frequency = (double) k * chromaBinHz;
				if (frequency < 40.0 || frequency > 2000.0)
					continue;
				const auto pitch = (int) std::lround (12.0 * std::log2 (frequency / 440.0)) + 69;
				chroma[(size_t) (pitch % 12)] += magnitude[k];
			}
			out.chroma.push_back (chroma);
		});
	}

	StemFeatures onGrid (const Frames& frames, const Grid& grid)
	{
		StemFeatures out;
		for (auto* perBeat : { &out.onset, &out.power })
			for (auto& band : *perBeat)
				band.assign ((size_t) grid.count, 0.0);

		const auto beatOf = [&grid] (double seconds) { return (int) std::floor ((seconds - grid.first) / grid.length); };
		std::vector<int> framesInBeat ((size_t) grid.count, 0);

		for (size_t t = 0; t < frames.flux[0].size(); ++t)
		{
			const auto centre = frames.firstCentre + (double) t / frames.rate;

			const auto nearest = (int) std::lround ((centre - grid.first) / grid.length);
			if (nearest >= 0 && nearest < grid.count && std::abs (centre - grid.at (nearest)) <= onsetReach)
				for (size_t b = 0; b < numBands; ++b)
					out.onset[b][(size_t) nearest] = std::max (out.onset[b][(size_t) nearest], (double) frames.flux[b][t]);

			const auto beat = beatOf (centre);
			if (beat >= 0 && beat < grid.count)
			{
				for (size_t b = 0; b < numBands; ++b)
					out.power[b][(size_t) beat] += frames.power[b][t];
				++framesInBeat[(size_t) beat];
			}
		}

		for (auto& band : out.power)
			for (size_t k = 0; k < band.size(); ++k)
				band[k] /= std::max (1, framesInBeat[k]);

		if (frames.chroma.empty())
			return out;

		out.chroma.assign ((size_t) grid.count, Chroma {});
		for (size_t t = 0; t < frames.chroma.size(); ++t)
		{
			const auto beat = beatOf (frames.chromaFirstCentre + (double) t / frames.chromaRate);
			if (beat >= 0 && beat < grid.count)
				for (size_t p = 0; p < 12; ++p)
					out.chroma[(size_t) beat][p] += frames.chroma[t][p];
		}
		return out;
	}

	//==========================================================================
	double mean (const std::vector<double>& values, int from, int to)
	{
		from = std::max (0, from);
		to = std::min ((int) values.size(), to);
		if (to <= from)
			return 0.0;
		return std::accumulate (values.begin() + from, values.begin() + to, 0.0) / (to - from);
	}

	double percentile (std::vector<double> values, double fraction)
	{
		if (values.empty())
			return 0.0;
		const auto index = (size_t) (fraction * (double) (values.size() - 1));
		std::nth_element (values.begin(), values.begin() + (long) index, values.end());
		return values[index];
	}

	std::vector<double> total (const std::array<std::vector<double>, numBands>& bands)
	{
		auto sum = bands[0];
		for (size_t b = 1; b < numBands; ++b)
			for (size_t k = 0; k < sum.size(); ++k)
				sum[k] += bands[b][k];
		return sum;
	}

	using BarPhase::PerBeatOfBar;
	using BarPhase::beatsPerBar;

	// How much more of something lands on each beat of the bar than on the
	// average beat: 0 everywhere when it is the same on every beat. Relative
	// to the average, but never to less than `scale`, what the measure counts
	// as a real amount -- or noise on a flat measure would look like evidence.
	PerBeatOfBar contrast (const std::vector<double>& perBeat, int from, int to, double scale)
	{
		PerBeatOfBar sums {};
		std::array<int, beatsPerBar> counts {};
		for (int k = std::max (0, from); k < std::min ((int) perBeat.size(), to); ++k)
		{
			sums[(size_t) (k % beatsPerBar)] += perBeat[(size_t) k];
			++counts[(size_t) (k % beatsPerBar)];
		}

		PerBeatOfBar result {};
		double average = 0.0;
		for (size_t o = 0; o < sums.size(); ++o)
		{
			if (counts[o] == 0)
				return result;
			sums[o] /= counts[o];
			average += sums[o] / beatsPerBar;
		}

		const auto reference = std::max (average, scale);
		if (reference <= 0.0)
			return result;

		for (size_t o = 0; o < sums.size(); ++o)
			result[o] = (sums[o] - average) / reference;
		return result;
	}

	// One vote per event for the beat of the bar it fell on.
	PerBeatOfBar votes (const std::vector<double>& events)
	{
		PerBeatOfBar result {};
		for (size_t k = 0; k < events.size(); ++k)
			result[k % beatsPerBar] += events[k];
		return result;
	}

	// A stem's level per beat as an amplitude, 1 for its usual loudness.
	std::vector<double> levels (const std::vector<double>& power)
	{
		const auto typical = std::max (percentile (power, 0.75), 1e-12);
		std::vector<double> level (power.size(), 0.0);
		for (size_t k = 0; k < power.size(); ++k)
			level[k] = std::min (2.0, std::sqrt (power[k] / typical));
		return level;
	}

	// Changes in a stem's level at a beat: the bar line is where parts come
	// in and drop out. Compared over a bar either side and squared, so that a
	// change smeared across a wrong bar line counts for less than a whole one.
	std::vector<double> levelChange (const std::vector<double>& level)
	{
		std::vector<double> change (level.size(), 0.0);
		for (int k = 1; k < (int) level.size(); ++k)
		{
			const auto step = mean (level, k, k + beatsPerBar) - mean (level, k - beatsPerBar, k);
			change[(size_t) k] = step * step;
		}
		return change;
	}

	// A part is gone for this long before it counts as coming back, or after
	// it stops: two bars.
	constexpr int breakBeats = 2 * beatsPerBar;
	constexpr double silentLevel = 0.2, playingLevel = 0.5;

	// The beats where a stem comes back after a break, and where it stops for
	// one (the first silent beat).
	std::vector<double> entriesAndExits (const std::vector<double>& level)
	{
		const auto count = (int) level.size();
		const auto loudest = [&level] (int from, int to)
		{
			return *std::max_element (level.begin() + from, level.begin() + to);
		};

		std::vector<double> events (level.size(), 0.0);
		for (int k = 1; k < count; ++k)
		{
			const auto entry = k >= breakBeats && level[(size_t) k] > playingLevel && loudest (k - breakBeats, k) < silentLevel;
			const auto exit = k + breakBeats <= count && level[(size_t) k - 1] > playingLevel && loudest (k, k + breakBeats) < silentLevel;
			events[(size_t) k] = (entry ? 1.0 : 0.0) + (exit ? 1.0 : 0.0);
		}
		return events;
	}

	// A crash: the top of the drums far louder than anywhere in the bar
	// before, where hats, snares and claps repeat every bar.
	std::vector<double> crashes (const std::vector<double>& highPower)
	{
		const auto typical = percentile (highPower, 0.75);
		std::vector<double> events (highPower.size(), 0.0);
		for (int k = beatsPerBar; k < (int) highPower.size(); ++k)
		{
			const auto before = *std::max_element (highPower.begin() + (k - beatsPerBar), highPower.begin() + k);
			if (highPower[(size_t) k] > 4.0 * before && highPower[(size_t) k] > 0.3 * typical)
				events[(size_t) k] = 1.0;
		}
		return events;
	}

	// The chord of the beats [from, to): pitch classes, unit length.
	Chroma chordOver (const std::vector<Chroma>& chroma, int from, int to)
	{
		Chroma sum {};
		for (int k = std::max (0, from); k < std::min ((int) chroma.size(), to); ++k)
			for (size_t p = 0; p < 12; ++p)
				sum[p] += chroma[(size_t) k][p];
		const auto norm = std::sqrt (std::inner_product (sum.begin(), sum.end(), sum.begin(), 0.0));
		if (norm > 0.0)
			for (auto& v : sum)
				v /= norm;
		return sum;
	}

	// How far the chord of the half bar from a beat is from the half bar before.
	std::vector<double> harmonicChange (const std::vector<Chroma>& chroma)
	{
		constexpr int halfBar = beatsPerBar / 2;
		std::vector<double> change (chroma.size(), 0.0);
		for (int k = halfBar; k + halfBar <= (int) chroma.size(); ++k)
		{
			const auto before = chordOver (chroma, k - halfBar, k);
			const auto after = chordOver (chroma, k, k + halfBar);
			change[(size_t) k] = 1.0 - std::inner_product (before.begin(), before.end(), after.begin(), 0.0);
		}
		return change;
	}

	// Bass and the other parts together, each stem at its own scale.
	std::vector<Chroma> harmony (const std::vector<const StemFeatures*>& stems, int count)
	{
		std::vector<Chroma> chroma ((size_t) count, Chroma {});
		for (const auto* f : stems)
		{
			double sum = 0.0;
			for (const auto& c : f->chroma)
				sum += std::accumulate (c.begin(), c.end(), 0.0);
			if (sum <= 0.0)
				continue;
			for (size_t k = 0; k < chroma.size(); ++k)
				for (size_t p = 0; p < 12; ++p)
					chroma[k][p] += f->chroma[k][p] * (double) count / sum;
		}
		return chroma;
	}

	std::vector<double> sum (std::vector<double> a, const std::vector<double>& b)
	{
		for (size_t k = 0; k < a.size() && k < b.size(); ++k)
			a[k] += b[k];
		return a;
	}

	// How much each kind of evidence counts, from 17 tracks whose one a DJ
	// had set by hand. Drums and bass coming in or stopping were on the one in
	// 12 of the 15 tracks that had any, so each such event outweighs the slow
	// measures, which decide where nothing came or went. Chord changes, the
	// arrangement, bass onsets and crashes each picked the one in about half
	// the tracks (chance is a quarter). Snare and kick patterns did no better
	// than chance and are not used.
	namespace weight
	{
		constexpr double entriesAndExits = 1.0;   // per event
		constexpr double harmony = 1.0;
		constexpr double arrangement = 1.0;
		constexpr double bassNotes = 0.5;
		constexpr double crashes = 0.25;          // per crash
	}

	struct Measured
	{
		std::array<Frames, BarPhase::numStems> frames;
		std::array<bool, BarPhase::numStems> present {};
	};

	bool measure (const BarPhase::Stems& stems, Measured& out, const std::function<bool()>& shouldAbort)
	{
		std::array<double, BarPhase::numStems> loudness {};

		for (size_t s = 0; s < stems.mono.size(); ++s)
		{
			if (stems.mono[s].size() < (size_t) chromaSize)
				continue;
			const auto withChroma = s == BarPhase::bass || s == BarPhase::other;
			if (! measureFrames (stems.mono[s], stems.sampleRate, withChroma, out.frames[s], shouldAbort))
				return false;
			for (const auto& band : out.frames[s].power)
				loudness[s] += std::accumulate (band.begin(), band.end(), 0.0) / (double) band.size();
		}

		// A stem far below the loudest is separation residue, not a part.
		const auto loudest = *std::max_element (loudness.begin(), loudness.end());
		for (size_t s = 0; s < loudness.size(); ++s)
			out.present[s] = loudest > 0.0 && loudness[s] > 1e-3 * loudest;
		return true;
	}

	// Beats tracked on the summed stems can lock onto the off-beat, where the
	// hats are -- they fill more of the spectrum than the kick. In four to
	// the floor the kick is on the beat, so the grid moves half a beat when
	// the drums' low end hits harder between its beats than on them.
	bool kickIsBetweenTheBeats (const Measured& measured, const Grid& grid)
	{
		if (! measured.present[BarPhase::drums])
			return false;

		auto halves = grid;
		halves.length = grid.length / 2;
		halves.count = grid.count * 2;
		const auto lowOnsets = onGrid (measured.frames[BarPhase::drums], halves).onset[low];

		double onBeats = 0.0, between = 0.0;
		for (size_t k = 0; k < lowOnsets.size(); ++k)
			(k % 2 == 0 ? onBeats : between) += lowOnsets[k];
		return between > onBeats;
	}

	Grid gridFor (double bpm, double beatPhase, double seconds)
	{
		Grid grid;
		grid.length = 60.0 / bpm;
		grid.first = beatPhase - std::floor (beatPhase / grid.length) * grid.length;
		grid.count = (int) std::floor ((seconds - grid.first) / grid.length);
		return grid;
	}
}

namespace BarPhase
{
	Result find (const Stems& stems, double bpm, double beatPhase, Beats beats, const std::function<bool()>& shouldAbort)
	{
		Result result;
		result.firstDownbeat = firstDownbeat (bpm > 0.0 ? bpm : 120.0, beatPhase, 0);
		if (bpm <= 0.0 || stems.sampleRate <= 0.0)
			return result;

		size_t longest = 0;
		for (const auto& s : stems.mono)
			longest = std::max (longest, s.size());

		auto grid = gridFor (bpm, beatPhase, (double) longest / stems.sampleRate);
		if (grid.count < 4 * beatsPerBar)
			return result;

		Measured measured;
		if (! measure (stems, measured, shouldAbort))
			return result;

		if (beats == Beats::ontoTheKick && kickIsBetweenTheBeats (measured, grid))
		{
			grid = gridFor (bpm, grid.first + grid.length / 2, (double) longest / stems.sampleRate);
			result.movedHalfABeat = true;
		}

		std::array<StemFeatures, numStems> features;
		for (size_t s = 0; s < features.size(); ++s)
			if (measured.present[s])
				features[s] = onGrid (measured.frames[s], grid);

		const auto note = [&result] (const char* name, const PerBeatOfBar& evidence, double w)
		{
			PerBeatOfBar weighted {};
			for (size_t o = 0; o < weighted.size(); ++o)
			{
				weighted[o] = w * evidence[o];
				result.scores[o] += weighted[o];
			}
			result.evidence.push_back ({ name, weighted });
		};

		std::vector<double> events ((size_t) grid.count, 0.0), arrangement ((size_t) grid.count, 0.0);
		for (size_t s = 0; s < features.size(); ++s)
		{
			if (! measured.present[s])
				continue;
			const auto level = levels (total (features[s].power));
			arrangement = sum (arrangement, levelChange (level));
			// Voices come in early (a pickup) and other parts fade, so only these.
			if (s == drums || s == bass)
				events = sum (events, entriesAndExits (level));
		}
		note ("in and out", votes (events), weight::entriesAndExits);
		note ("arrangement", contrast (arrangement, beatsPerBar, grid.count - beatsPerBar, 0.05), weight::arrangement);

		std::vector<const StemFeatures*> harmonic;
		for (const auto s : { bass, other })
			if (measured.present[(size_t) s])
				harmonic.push_back (&features[(size_t) s]);
		if (! harmonic.empty())
			note ("harmony", contrast (harmonicChange (harmony (harmonic, grid.count)), beatsPerBar / 2,
									   grid.count - beatsPerBar / 2, 0.1),
				  weight::harmony);

		if (measured.present[bass])
			note ("bass notes", contrast (sum (features[bass].onset[low], features[bass].onset[mid]), 0, grid.count, 0.0),
				  weight::bassNotes);

		if (measured.present[drums])
			note ("crashes", votes (crashes (features[drums].power[high])), weight::crashes);

		result.offset = (int) std::distance (result.scores.begin(), std::max_element (result.scores.begin(), result.scores.end()));
		result.firstDownbeat = firstDownbeat (bpm, grid.first, result.offset);
		return result;
	}

	double foundAgain (const Stems& stems, double bpm, double firstBeat, const std::function<bool()>& shouldAbort)
	{
		return find (stems, bpm, firstBeat, Beats::ontoTheKick, shouldAbort).firstDownbeat;
	}

	double firstDownbeat (double bpm, double beatPhase, int offset)
	{
		const auto beatLength = 60.0 / bpm;
		const auto bar = beatsPerBar * beatLength;
		const auto firstBeat = beatPhase - std::floor (beatPhase / beatLength) * beatLength;
		const auto downbeat = firstBeat + offset * beatLength;
		return downbeat - std::floor (downbeat / bar) * bar;
	}
}
