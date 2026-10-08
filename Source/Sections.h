#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

// What a set does bar by bar -- groove, build, drop, breakdown -- read from
// its stems (spec fpv-pilots, phase A). Pure: no JUCE, testable.
//
// Two steps, because they change at different times. The features are
// measured once per set from the audio and cached: per beat of the analysed
// grid, each stem's level and how many attacks it had. The sections are
// worked out from them whenever the grid's downbeat moves (Grid Adjust), in
// microseconds, without reading a file.
namespace sections
{
	constexpr int numStems = 4;
	constexpr int beatsPerBar = 4;

	struct Frame
	{
		std::array<float, numStems> rms {};      // each stem's level over the frame
		std::array<float, numStems> onsets {};   // each stem's attacks in the frame
	};

	// Frame i covers [startSeconds + i * hopSeconds, ... + hopSeconds).
	struct Features
	{
		double startSeconds = 0.0, hopSeconds = 0.0;
		std::vector<Frame> frames;
	};

	// An attack: a 10 ms slice of a stem's high band (its first difference,
	// which keeps hats and snares and mostly drops the bass) above onsetFloor
	// and onsetRise times louder than the loudest of the onsetHistory slices
	// before it -- and not within those slices of the last attack, so one
	// hit that straddles two slices counts once.
	constexpr double sliceSeconds = 0.01;
	constexpr int onsetHistory = 3;
	constexpr float onsetRise = 2.0f;
	constexpr float onsetFloor = 1.0e-3f;   // -60 dBFS

	// Fed the four stems block by block, in step, as they are read.
	class FeatureBuilder
	{
	public:
		FeatureBuilder (double sampleRate, double startSeconds, double hopSeconds);

		void add (const std::array<const float*, numStems>& stems, int count);
		Features finish();

	private:
		void closeSlice();
		void ensureFrame (long long frame);

		double sampleRate, startSeconds, hopSeconds;
		long long sampleIndex = 0, currentSlice = 0;
		std::array<float, numStems> previous {};
		std::array<double, numStems> sliceSquares {};
		long long sliceSamples = 0;
		std::array<std::vector<float>, numStems> history;
		std::array<int, numStems> slicesSinceOnset { onsetHistory, onsetHistory, onsetHistory, onsetHistory };
		std::vector<std::array<double, numStems>> frameSquares;
		std::vector<long long> frameSamples;
		std::vector<std::array<float, numStems>> frameOnsets;
	};

	// The cache file's text, and back. Nothing for text that is not one.
	std::string encode (const Features& features);
	std::optional<Features> decode (const std::string& text);

	// Which stem is the drums and which the bass: by name where the names
	// say so ("drums", "kick" / "bass"), else the stem creator's order
	// (1 drums, 2 bass, 3 other, 4 vocals) -- never both on one stem.
	struct StemRoles
	{
		int drums = 0, bass = 1;
	};

	StemRoles rolesFor (const std::array<std::string, numStems>& stemNames);

	struct BarLevels
	{
		float drums = 0.0f, bass = 0.0f, total = 0.0f;
		float drumOnsetsPerBeat = 0.0f;
	};

	// The frames grouped into the bars of the deck's grid. Bar 0 starts on
	// `firstBeat`, the downbeat -- corrected by hand or as analysed; a frame
	// belongs to the bar its middle falls in. What lies before the downbeat
	// (a pickup) is in no bar. No grid (bpm 0): no bars.
	std::vector<BarLevels> barLevels (const Features& features, double bpm, double firstBeat, StemRoles roles);
}
