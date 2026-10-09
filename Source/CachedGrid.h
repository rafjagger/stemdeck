#pragma once

// What the analysis cache holds for a set, and when its bar's one is found
// again. Pure: no JUCE, testable.
//
// Grids analysed before the one was found from the stems took the track's
// first beat as the one, and their beats could sit on the off-beat. Their
// tempo stays; the next time such a set is loaded its first beat is found
// again (BarPhase::foundAgain). A grid the DJ corrected by hand is his and is
// never touched.
namespace CachedGrid
{
	// 1: the first beat was the one. 2: the one found from the stems.
	constexpr int currentVersion = 2;

	struct Entry
	{
		double bpm = 0.0;
		double firstBeat = 0.0;
		int version = 1;         // entries from before versions were written have none
		bool corrected = false;  // a grid corrected by hand is kept beside this one
	};

	bool needsNewDownbeat (const Entry& entry);

	// The entry with the one found again at `firstBeat`; unchanged if it was
	// corrected by hand meanwhile.
	Entry withNewDownbeat (Entry entry, double firstBeat);
}
