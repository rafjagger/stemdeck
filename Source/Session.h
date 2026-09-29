#pragma once

#include <JuceHeader.h>
#include "StemSet.h"

#include <array>
#include <vector>

// What StemDeck remembers across a restart: both decks as they were, playing
// ones playing on from where they were, the mixer, the library view and the
// stem creator's queue. Written every couple of seconds and on quit to
// ~/.config/StemDeck/session.xml, so a crash or a power cut loses at most
// that much.

struct StemMixState
{
	double gainDb = 0.0;
	bool muted = false;
	unsigned buses = 0;     // bit per bus (Buses.h)
};

struct DeckSession
{
	juce::String setId;     // the set's first stem file, relative to the library; empty: deck empty
	double position = 0.0;
	bool playing = false;
	double cuePoint = 0.0;
	juce::Range<double> loop;   // empty: no loop
	bool repeat = false;

	double tempo = 1.0;         // the fader's ratio
	double tempoRange = 0.08;
	bool vinyl = true;
	bool sync = false;

	std::array<StemMixState, StemSet::numStems> stems;
	double faderDb = 0.0;
	bool phones = false;
};

struct LibrarySession
{
	int sortColumn = 0;         // 0: leave as it is
	bool sortForwards = true;
	juce::String search;
	juce::String selectedSetId;
};

struct QueuedStemJob
{
	juce::String input, folder, track;
};

struct Session
{
	std::array<DeckSession, 2> decks;
	int masterDeck = -1;
	bool masterTurnedOff = false;
	bool autoDj = false;
	LibrarySession library;
	std::vector<QueuedStemJob> stemJobs;

	std::unique_ptr<juce::XmlElement> toXml() const;
	static Session fromXml (const juce::XmlElement& xml);

	static juce::File file (const juce::File& settingsFile) { return settingsFile.getSiblingFile ("session.xml"); }
	static std::optional<Session> load (const juce::File& file);
};
