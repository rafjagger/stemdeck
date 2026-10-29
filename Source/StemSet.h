#pragma once

#include <JuceHeader.h>
#include "StemNames.h"
#include <array>
#include <vector>

// A track made of exactly four stereo stems that belong together.
// Files are grouped by their common name prefix, e.g.
//   "Artist - Title-001.wav" ... "Artist - Title-004.wav"
//   "Artist - Title - 01.wav" ... "Artist - Title - 04.wav"
//   "Artist - Title - DUB.wav", "... - KICK.wav", "... - PADS.wav", "... - PERC.wav"
// The stems are ordered by their suffix; stem N is played on output pair N.
struct StemSet
{
	static constexpr int numStems = 4;

	juce::String name;
	// Where it sits in the library: stems/Artist/Album/<files> (LibraryPath.h).
	juce::String artist, album;
	// Its folder below the library folder, '/'-separated; "" straight in it.
	juce::String folder;
	std::array<juce::File, numStems> files;
	std::array<juce::String, numStems> stemNames;
	double lengthSeconds = 0.0; // from the first stem's header

	// The stems' names as shown on screen (stemDisplayNames, StemNames.h):
	// "drums" for "1 - drums", LOW ... VOCALS for a space4 set. Inline: the
	// test targets that build the deck views do not link StemSet.cpp.
	std::array<juce::String, numStems> displayNames() const
	{
		std::array<std::string, numStems> suffixes;
		for (size_t s = 0; s < suffixes.size(); ++s)
			suffixes[s] = stemNames[s].toStdString();

		const auto shown = stemDisplayNames (suffixes);
		std::array<juce::String, numStems> names;
		for (size_t s = 0; s < names.size(); ++s)
			names[s] = juce::String (shown[s]);
		return names;
	}

	// Scans a folder (recursively) for complete four-stem sets, sorted by
	// artist, album and name; artist and album come from the folders the set
	// lies in below `folder`.
	static std::vector<StemSet> scanFolder (const juce::File& folder, juce::AudioFormatManager& formats);
};
