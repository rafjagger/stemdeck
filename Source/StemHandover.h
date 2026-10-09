#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

// Where the Auto-DJ hands a stem over from the old track to the new one.
// Pure: no JUCE, testable.
//
// Each stem's loudness over its track is an envelope (RMS per 50 ms). A
// changeover at a downbeat is least heard where
//   - the old stem goes quiet just after it (a rest, the end of a phrase:
//     nothing is cut off), measured against just before it,
//   - the new stem was quiet just before it in its own track (it enters at
//     the start of a phrase, not in the middle of a note),
//   - and the old stem is at its quietest of the candidates.
// "Just before / after" is half the window each way, the nearest moments
// weighted most (a triangle), so a rest right on the downbeat counts more
// than one three seconds later.
namespace StemHandover
{
	constexpr int numStems = 4;                // 0 drums, 1 bass, 2 other, 3 vocals
	constexpr int drums = 0, bass = 1, other = 2, vocals = 3;
	constexpr double envelopeHop = 0.05;       // seconds
	constexpr double window = 8.0;             // seconds around a downbeat

	struct Envelope
	{
		double hop = envelopeHop;
		std::vector<float> rms;

		// The level at a track time; silence outside the track.
		float levelAt (double seconds) const;
	};
	using Envelopes = std::array<Envelope, numStems>;

	// Builds a stem's envelope from blocks of its audio, as it is read.
	class EnvelopeBuilder
	{
	public:
		explicit EnvelopeBuilder (double sampleRate, double hop = envelopeHop);
		void add (const float* const* channels, int numChannels, int numSamples);
		Envelope finish();

	private:
		Envelope envelope;
		std::size_t samplesPerHop;
		std::size_t filled = 0;     // samples in the running hop
		double squares = 0.0;
		int channelsSeen = 1;
	};

	// A downbeat both tracks share during the overlap: its number from the
	// start of the overlap (0: where the new track starts) and where it falls
	// in each track's own seconds.
	struct Downbeat
	{
		int bar = 0;
		double oldSeconds = 0.0, newSeconds = 0.0;
	};

	// How audible the changeover of one stem would be at each candidate,
	// 0 (unheard) .. 3, in the order of `candidates`.
	std::vector<double> changeoverCosts (const Envelope& oldStem, const Envelope& newStem,
										 const std::vector<Downbeat>& candidates, double windowSeconds);

	// The least audible of the candidates (its bar); the earliest on a tie.
	// -1 without candidates.
	int chooseDownbeat (const Envelope& oldStem, const Envelope& newStem,
						const std::vector<Downbeat>& candidates, double windowSeconds);

	// The bars where bass and other change over, in an overlap whose vocals
	// change over at `endBar` (the drums did at bar 0). Never on the same
	// downbeat, and spread: each at least a fifth of the overlap (at least a
	// bar) from the drums, the vocals and each other. Bass before other --
	// the new groove (drums and bass) settles first, the old track stays
	// recognisable by its melody -- unless the analysis clearly prefers the
	// other way round. Without anything to hear (no envelopes) they land
	// near a third and two thirds of the overlap. An overlap too short for
	// both inside it puts what does not fit at `endBar`, with the vocals.
	struct Pair
	{
		int bass = 0, other = 0;
	};
	// Where a track is heard, in its own seconds: from the first sound of all
	// four stems together to the last, the silence around it left out.
	// "Heard" is less than 50 dB under the track's loud parts (the level 95 %
	// of it stays under) -- a fade-out counts until it is that far down -- and
	// sustained: at least 0.3 s of the following (or, for the end, the
	// preceding) second, so a click in the silence is not the track. Nothing
	// when there is nothing to hear.
	struct Span
	{
		double start = 0.0, end = 0.0;
	};
	constexpr double audibleBelowLoudDb = 50.0;
	std::optional<Span> audibleSpan (const Envelopes& stems);

	Pair chooseBassAndOther (const Envelopes& oldStems, const Envelopes& newStems,
							 const std::vector<Downbeat>& downbeats, int endBar, double windowSeconds);
}
