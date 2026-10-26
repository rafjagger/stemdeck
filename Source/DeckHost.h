// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include "GridAction.h"
#include "DeckSpatialPorts.h"
#include "Session.h"
#include "StemDeckPlayer.h"
#include "TempoAnalysis.h"

#include <functional>
#include <optional>
#include <vector>

// One deck as every screen needs it: its player, its spatial ports, the set
// it holds, the beat grid found for it in the background (and corrected by
// hand), its sections, and its part of the session. Two of them make the
// two-deck screen; the one-deck screen (-bformat) has one. What spans decks
// -- the buses, sync, Auto DJ, the master -- is not here.
//
// The analysis pool and the cache are shared by the decks of a screen. The
// analysis reports on the message thread; a result for a set no longer
// loaded is dropped.
class DeckHost
{
public:
	// The cache lives beside the app's settings and outlives the deck. The
	// layout: where the deck's spatial ports lie in what render() writes.
	DeckHost (juce::AudioFormatManager&, juce::ThreadPool& analysisPool, AnalysisCache&,
	          outputs::SpatialLayout = outputs::twoDeckSpatialLayout());
	~DeckHost();

	// The deck panel's "analysing" light; and a grid stored in the cache,
	// which the library shows.
	std::function<void (bool analysing)> onAnalysing;
	std::function<void()> onAnalysisStored;

	StemDeckPlayer& player() { return deckPlayer; }
	const StemDeckPlayer& player() const { return deckPlayer; }
	DeckSpatialPorts& spatialPorts() { return ports; }
	const DeckSpatialPorts& spatialPorts() const { return ports; }

	// Builds the spatial engine; an error, and silent spatial ports, if IEM refuses.
	juce::String createSpatial() { return ports.create(); }

	//==============================================================================
	// Audio: allocates the deck's stem pairs, then fills them each block and
	// writes the layout's channels from `firstChannel` on in `spatialOutputs`.
	void prepare (double sampleRate, int samplesPerBlockExpected);
	const juce::AudioBuffer<float>& render (juce::AudioBuffer<float>& spatialOutputs, int firstChannel, int numSamples);
	void releaseResources() { deckPlayer.releaseResources(); }

	//==============================================================================
	// Loads the set and starts on its grid: from the cache, else analysed.
	// An error, and nothing changed, if the player refuses it.
	juce::String load (const StemSet& set);
	const std::optional<StemSet>& loadedSet() const { return set; }
	const juce::String& loadedSetId() const { return setId; }   // StemLibrary's id: the first stem's path
	int loadGeneration() const { return generation; }            // counts loads: results for an older one are dropped
	const std::vector<sections::Bar>& sectionBars() const { return bars; }

	// Grid Adjust (GridAction): the edit kept in the cache as a
	// correction, RESET back to the analysed grid. SHIFT GRID needs the
	// leader's beat phase now (the other deck, or the Pioneer master);
	// without it nothing happens. True if the grid was changed.
	bool editGrid (GridAction action, double seconds, std::optional<double> leaderBeatPhase);

	//==============================================================================
	// The deck's part of the session: where it was, its loop and cue, each
	// stem's spatial values. The set itself, the panel and the mixer strip
	// are the screen's.
	void gather (DeckSession&) const;
	void restoreSpatial (const DeckSession&);     // before the set is loaded
	void restoreTransport (const DeckSession&);   // after it is

private:
	void startAnalysis();
	void startDownbeatAnalysis();
	void startSectionAnalysis (const BeatGrid& grid);
	void rebuildSections();
	void setAnalysing (bool analysing);
	void analysisStored();

	juce::AudioFormatManager& formats;
	juce::ThreadPool& pool;
	AnalysisCache& analysisCache;

	StemDeckPlayer deckPlayer { formats };
	DeckSpatialPorts ports;
	juce::AudioBuffer<float> stemPairs;   // StemDeckPlayer::numOutputChannels

	std::optional<StemSet> set;
	juce::String setId;
	int generation = 0;
	std::optional<sections::Features> features;
	std::vector<sections::Bar> bars;

	// What the session held for each stem's spatial values, kept for a deck
	// without a spatial engine, which would otherwise save the defaults.
	decltype (DeckSession::stems) restoredSpatial {};

	JUCE_DECLARE_WEAK_REFERENCEABLE (DeckHost)
	JUCE_DECLARE_NON_COPYABLE (DeckHost)
};
