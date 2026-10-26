// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "TempoAnalysis.h"

// Two -bformat instances share analysis.xml. Each writes only the sets it
// touched into the file as it is now, so neither drops the other's new
// analyses and grid corrections.
namespace
{
StemSet setNamed (const juce::String& name)
{
	StemSet set;
	set.name = name;
	for (int s = 0; s < StemSet::numStems; ++s)
		set.files[(size_t) s] = juce::File ("/nonexistent/" + name + "-" + juce::String (s) + ".flac");
	return set;
}

struct Folder
{
	Folder() { folder.createDirectory(); }
	~Folder() { folder.deleteRecursively(); }
	juce::File folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("stemdeck-cache", "");
	juce::File file = folder.getChildFile ("analysis.xml");
};

void expectGrid (const std::optional<BeatGrid>& found, double bpm, double firstBeat, const char* what)
{
	ASSERT_TRUE (found.has_value()) << what;
	EXPECT_DOUBLE_EQ (found->bpm, bpm) << what;
	EXPECT_DOUBLE_EQ (found->firstBeat, firstBeat) << what;
}
}

TEST (AnalysisCache, TwoInstancesKeepEachOthersEntries)
{
	Folder f;
	const auto setA = setNamed ("a"), setB = setNamed ("b"), shared = setNamed ("shared");
	{
		AnalysisCache seed (f.file);
		seed.store (shared, { 120.0, 0.1 });
	}

	AnalysisCache first (f.file), second (f.file);   // both started on the same file

	// Each analyses one set and corrects the shared one; neither knows of the other.
	first.store (setA, { 124.0, 0.2 });
	second.store (setB, { 128.0, 0.3 });
	first.storeCorrected (shared, { 121.0, 0.11 });
	first.flush();
	second.storeCorrected (shared, { 122.0, 0.12 });
	second.flush();

	AnalysisCache third (f.file);
	expectGrid (third.find (setA), 124.0, 0.2, "the first instance's analysis");
	expectGrid (third.find (setB), 128.0, 0.3, "the second instance's analysis");
	expectGrid (third.find (shared), 122.0, 0.12, "the same set corrected in both: the last flush wins");
	expectGrid (third.findAnalysed (shared), 120.0, 0.1, "and its analysed grid stays");

	// What the other one wrote is taken into memory too.
	expectGrid (second.find (setA), 124.0, 0.2, "adopted on the write");
}

// A correction in one instance survives an analysis stored by the other.
TEST (AnalysisCache, AnAnalysisElsewhereKeepsACorrectionHere)
{
	Folder f;
	const auto setA = setNamed ("a"), setB = setNamed ("b");
	{
		AnalysisCache seed (f.file);
		seed.store (setA, { 124.0, 0.2 });
	}
	AnalysisCache first (f.file), second (f.file);
	first.storeCorrected (setA, { 125.0, 0.25 });
	first.flush();
	second.store (setB, { 128.0, 0.3 });   // written at once, whole file

	AnalysisCache third (f.file);
	expectGrid (third.find (setA), 125.0, 0.25, "the correction");
	expectGrid (third.find (setB), 128.0, 0.3, "the analysis");
}
