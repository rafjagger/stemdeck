#pragma once

#include <JuceHeader.h>
#include "StemSet.h"
#include "Sections.h"
#include "BarPhase.h"
#include "CachedGrid.h"
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
	// (as Mixxx does by default), puts the beats on the kick, then finds which
	// beat is the bar's one. Runs for a few seconds; call it off the message
	// thread. Returns an invalid grid if aborted or nothing rhythmic was found.
	BeatGrid analyse (const StemSet& set, juce::AudioFormatManager& formatManager,
					  const std::function<bool()>& shouldAbort = [] { return false; });

	// A cached grid's one found anew: the tempo stays, the beats move half a
	// beat onto the kick if they were between, the first beat onto the one.
	// Invalid if aborted.
	BeatGrid redetectDownbeat (const StemSet& set, juce::AudioFormatManager& formatManager, const BeatGrid& grid,
							   const std::function<bool()>& shouldAbort = [] { return false; });

	// The steps of the above, exposed for testing and measuring.
	// Each stem to mono at ~11 kHz; nothing if a file cannot be read or aborted.
	std::optional<BarPhase::Stems> readStems (const StemSet& set, juce::AudioFormatManager& formatManager,
											  const std::function<bool()>& shouldAbort = [] { return false; });
	// Tempo and beat phase from all stems summed; its first beat is the
	// earliest beat, not yet the bar's one.
	BeatGrid analyseBeats (const BarPhase::Stems& stems, const std::function<bool()>& shouldAbort = [] { return false; });
	// The grid's first beat moved onto the earliest downbeat -- and, with
	// Beats::ontoTheKick, the beats half a beat onto the kick if they were
	// between.
	BeatGrid findDownbeat (const BarPhase::Stems& stems, BeatGrid grid, BarPhase::Beats beats,
						   const std::function<bool()>& shouldAbort = [] { return false; });

	// Tempo and beat phase of a mono signal.
	BeatGrid analyseMono (const std::vector<float>& samples, double sampleRate,
						  const std::function<bool()>& shouldAbort = [] { return false; });
}

// Remembers analysis results per set (keyed by the stem files' names, sizes
// and dates -- not their folder, so moving the library keeps them) in a small
// XML file, so a set is only analysed once.
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

	// A grid analysed before the one was found from the stems, never
	// corrected by hand: its one is to be found again (CachedGrid.h).
	bool needsNewDownbeat (const StemSet& set) const;
	// The analysed grid's first beat, found again -- the one, and the beats
	// moved onto the kick with it; a correction made meanwhile wins.
	void storeNewDownbeat (const StemSet& set, double firstBeat);
	// Corrections are many small steps (the jog): kept in memory, written here.
	void flush();

	// The set's section features (Sections.h), in a text file of their own
	// beside this one (sections/<key>.txt): a few hundred lines a set would
	// make this XML slow to write on every grid correction.
	std::optional<sections::Features> findFeatures (const StemSet& set) const;
	void storeFeatures (const StemSet& set, const sections::Features& features);

private:
	static juce::String keyFor (const StemSet& set);
	juce::File featuresFileFor (const StemSet& set) const;
	static CachedGrid::Entry entryOf (const juce::XmlElement& element);
	void write();

	juce::File file;
	std::unique_ptr<juce::XmlElement> xml;
	bool unwritten = false;
};
