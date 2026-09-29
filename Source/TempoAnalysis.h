#pragma once

#include <JuceHeader.h>
#include "StemSet.h"
#include <optional>

// Constant-tempo beat grid: beats at firstBeat + n * 60 / bpm (seconds).
struct BeatGrid
{
	double bpm = 0.0;
	double firstBeat = 0.0;

	bool isValid() const { return bpm > 0.0; }
	double beatLength() const { return 60.0 / bpm; }

	// Position in beats (fractional) for a track time.
	double beatsAt (double seconds) const { return (seconds - firstBeat) / beatLength(); }
};

namespace TempoAnalysis
{
	// Estimates tempo and beat phase of a stem set, assuming a constant tempo
	// (as Mixxx does by default). All four stems are summed, so drums on any
	// stem count. Runs for a few seconds; call it off the message thread.
	// Returns an invalid grid if aborted or nothing rhythmic was found.
	BeatGrid analyse (const StemSet& set, juce::AudioFormatManager& formatManager,
					  const std::function<bool()>& shouldAbort = [] { return false; });

	// Core of the above, on a mono signal. Exposed for testing.
	BeatGrid analyseMono (const std::vector<float>& samples, double sampleRate,
						  const std::function<bool()>& shouldAbort = [] { return false; });
}

// Remembers analysis results per set (keyed by the stem files' paths, sizes
// and dates) in a small XML file, so a set is only analysed once.
class AnalysisCache
{
public:
	explicit AnalysisCache (const juce::File& file);

	// The grid to use: corrected by hand if it was, else as analysed.
	std::optional<BeatGrid> find (const StemSet& set) const;
	void store (const StemSet& set, const BeatGrid& grid);   // the analysis

	// A grid corrected by hand (Grid Adjust), kept beside the analysed one;
	// clearing it brings the analysed one back (RESET).
	void storeCorrected (const StemSet& set, const BeatGrid& grid);
	void clearCorrected (const StemSet& set);
	std::optional<BeatGrid> findAnalysed (const StemSet& set) const;
	// Corrections are many small steps (the jog): kept in memory, written here.
	void flush();

private:
	static juce::String keyFor (const StemSet& set);

	juce::File file;
	std::unique_ptr<juce::XmlElement> xml;
	bool unwritten = false;
};
