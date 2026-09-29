#pragma once

#include <JuceHeader.h>
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
	std::array<juce::File, numStems> files;
	std::array<juce::String, numStems> stemNames;
	double lengthSeconds = 0.0; // from the first stem's header

	// Scans a folder (recursively) for complete four-stem sets, sorted by
	// artist, album and name; artist and album come from the folders the set
	// lies in below `folder`.
	static std::vector<StemSet> scanFolder (const juce::File& folder, juce::AudioFormatManager& formats);
};
