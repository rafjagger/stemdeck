// How often the bar's one is found where a DJ put it by hand: every set in
// an analysis cache with a corrected grid, found under a stems folder, is
// analysed again -- the beats, the old one (the first beat) and the new one --
// and set against the correction.
//
//   stemdeck-downbeat-eval <analysis.xml> <stems folder> [decoded-audio folder] [-v]
//
// The decoded-audio folder keeps the stems as read, so a second run skips
// decoding. -v prints what each kind of evidence said.

#include <JuceHeader.h>
#include "TempoAnalysis.h"

#include <cstdio>
#include <map>

namespace
{
	constexpr double hitReach = 0.15;   // beats either side of the correction

	struct Truth
	{
		double bpm = 0.0, firstBeat = 0.0;
	};

	std::map<juce::String, Truth> correctedSets (const juce::File& analysisFile)
	{
		std::map<juce::String, Truth> truths;
		const auto xml = juce::XmlDocument::parse (analysisFile);
		if (xml == nullptr)
			return truths;
		for (auto* entry : xml->getChildIterator())
			if (entry->hasAttribute ("correctedBpm") && entry->hasAttribute ("correctedFirstBeat"))
				truths[entry->getStringAttribute ("name")] = { entry->getDoubleAttribute ("correctedBpm"),
															   entry->getDoubleAttribute ("correctedFirstBeat") };
		return truths;
	}

	// Where a detected first beat lies in the corrected bar, in beats [0, 4).
	double beatsFromTruth (double firstBeat, const Truth& truth)
	{
		const auto beats = (firstBeat - truth.firstBeat) * truth.bpm / 60.0;
		const auto bar = (double) BarPhase::beatsPerBar;
		return beats - std::floor (beats / bar) * bar;
	}

	bool isHit (double offset)
	{
		return std::min (offset, BarPhase::beatsPerBar - offset) <= hitReach;
	}

	juce::File decodedFile (const juce::File& folder, const StemSet& set)
	{
		return folder.getChildFile (juce::String::toHexString (set.name.hashCode64()) + ".stems");
	}

	std::optional<BarPhase::Stems> loadDecoded (const juce::File& file)
	{
		juce::FileInputStream in (file);
		if (! in.openedOk())
			return std::nullopt;
		BarPhase::Stems stems;
		stems.sampleRate = in.readDouble();
		for (auto& mono : stems.mono)
		{
			mono.resize ((size_t) in.readInt64());
			in.read (mono.data(), (int) (mono.size() * sizeof (float)));
		}
		return stems;
	}

	void saveDecoded (const juce::File& file, const BarPhase::Stems& stems)
	{
		file.getParentDirectory().createDirectory();
		juce::FileOutputStream out (file);
		if (! out.openedOk())
			return;
		out.setPosition (0);
		out.truncate();
		out.writeDouble (stems.sampleRate);
		for (const auto& mono : stems.mono)
		{
			out.writeInt64 ((juce::int64) mono.size());
			out.write (mono.data(), mono.size() * sizeof (float));
		}
	}

	std::optional<BarPhase::Stems> stemsOf (const StemSet& set, juce::AudioFormatManager& formats, const juce::File& decoded)
	{
		if (decoded != juce::File())
			if (auto cached = loadDecoded (decodedFile (decoded, set)))
				return cached;
		auto stems = TempoAnalysis::readStems (set, formats);
		if (stems && decoded != juce::File())
			saveDecoded (decodedFile (decoded, set), *stems);
		return stems;
	}

	void printEvidence (const BarPhase::Result& result)
	{
		for (const auto& [name, scores] : result.evidence)
			std::printf ("      %-12s %6.2f %6.2f %6.2f %6.2f\n", name.c_str(), scores[0], scores[1], scores[2], scores[3]);
	}
}

int main (int argc, char* argv[])
{

	juce::StringArray args;
	for (int i = 1; i < argc; ++i)
		args.add (argv[i]);
	const auto verbose = args.contains ("-v");
	args.removeString ("-v");

	if (args.size() < 2)
	{
		std::fprintf (stderr, "usage: stemdeck-downbeat-eval <analysis.xml> <stems folder> [decoded-audio folder] [-v]\n");
		return 2;
	}

	const auto truths = correctedSets (juce::File::getCurrentWorkingDirectory().getChildFile (args[0]));
	const auto stemsFolder = juce::File::getCurrentWorkingDirectory().getChildFile (args[1]);
	const auto decoded = args.size() > 2 ? juce::File::getCurrentWorkingDirectory().getChildFile (args[2]) : juce::File();

	juce::AudioFormatManager formats;
	formats.registerBasicFormats();

	// Columns: the corrected tempo and the detected one; how far the detected
	// beats are off the corrected ones (beats, -0.5 .. 0.5); then where each
	// way's first downbeat lies in the corrected bar (beats, 0 .. 4, a hit
	// near 0 or 4): old = the first beat, new = a fresh analysis, kept = a
	// cached grid's one found again with its beats kept, true = the new
	// detection on the corrected beats (the bar alone).
	int sets = 0, tempoMisses = 0;
	std::array<int, 4> hits {};
	std::printf ("%-40s %8s %8s %6s %6s %6s %6s %6s  %s\n", "set", "true", "bpm", "beats", "old", "new", "kept", "true",
				 "old/new/kept/true");

	for (const auto& set : StemSet::scanFolder (stemsFolder, formats))
	{
		const auto truth = truths.find (set.name);
		if (truth == truths.end())
			continue;

		const auto stems = stemsOf (set, formats, decoded);
		if (! stems)
		{
			std::printf ("%-40s cannot be read\n", set.name.substring (0, 40).toRawUTF8());
			continue;
		}

		++sets;
		const auto started = juce::Time::getMillisecondCounterHiRes();
		const auto beats = TempoAnalysis::analyseBeats (*stems);
		const auto found = BarPhase::find (*stems, beats.bpm, beats.firstBeat, BarPhase::Beats::ontoTheKick);
		const auto elapsed = (juce::Time::getMillisecondCounterHiRes() - started) / 1000.0;
		const auto kept = BarPhase::find (*stems, beats.bpm, beats.firstBeat, BarPhase::Beats::keep);
		const auto onTruth = BarPhase::find (*stems, truth->second.bpm, truth->second.firstBeat, BarPhase::Beats::keep);

		const std::array<double, 4> offsets { beatsFromTruth (beats.firstBeat, truth->second),
											  beatsFromTruth (found.firstDownbeat, truth->second),
											  beatsFromTruth (kept.firstDownbeat, truth->second),
											  beatsFromTruth (onTruth.firstDownbeat, truth->second) };
		const auto beatError = offsets[0] - std::round (offsets[0]);
		const auto tempoRight = std::abs (beats.bpm - truth->second.bpm) <= 0.05;
		tempoMisses += tempoRight ? 0 : 1;

		juce::String verdicts;
		for (size_t i = 0; i < offsets.size(); ++i)
		{
			hits[i] += isHit (offsets[i]) ? 1 : 0;
			verdicts << (i > 0 ? "/" : "") << (isHit (offsets[i]) ? "hit" : "miss");
		}

		std::printf ("%-40s %8.3f %8.3f %6.2f %6.2f %6.2f %6.2f %6.2f  %s%s%s  (%.1f s)\n", set.name.substring (0, 40).toRawUTF8(),
					 truth->second.bpm, beats.bpm, beatError, offsets[0], offsets[1], offsets[2], offsets[3],
					 verdicts.toRawUTF8(), found.movedHalfABeat ? "  moved" : "", tempoRight ? "" : "  TEMPO", elapsed);

		if (verbose)
		{
			std::printf ("    on the detected beats:\n");
			printEvidence (found);
			const auto beatLength = 60.0 / truth->second.bpm;
			std::printf ("    on the corrected beats (the one is beat %d):\n",
						 ((int) std::floor (truth->second.firstBeat / beatLength + 1e-6)) % BarPhase::beatsPerBar);
			printEvidence (onTruth);
		}
	}

	std::printf ("\n%d sets, hits: old %d, new %d, kept %d, on the corrected beats %d; tempo off in %d\n",
				 sets, hits[0], hits[1], hits[2], hits[3], tempoMisses);
	return 0;
}
