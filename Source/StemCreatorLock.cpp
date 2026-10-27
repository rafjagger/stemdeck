// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "StemCreatorLock.h"

namespace
{
constexpr int journalLines = 200;   // what is kept; a waiting job is minutes old, not hundreds of jobs

juce::String journalKey (const juce::File& input, const juce::String& folder, const juce::String& track)
{
	return input.getFullPathName() + "\t" + folder + "\t" + track;
}
}

StemCreatorLock::StemCreatorLock (const juce::File& folder)
	: libraryFolder (folder), lock (folder.getChildFile ("stemcreator.lock"))
{
}

StemCreatorLock::~StemCreatorLock() = default;

StemCreatorLock::Result StemCreatorLock::tryAcquire()
{
	return libraryFolder.isDirectory() ? lock.tryAcquire() : Result::unavailable;
}

// "<ms>\t<input>\t<folder>\t<track>" a line, the newest last.
void StemCreatorLock::recordDone (const juce::File& input, const juce::String& folder, const juce::String& track)
{
	const auto journal = libraryFolder.getChildFile ("stemcreator.done");
	juce::StringArray lines;
	lines.addLines (journal.loadFileAsString());
	lines.removeEmptyStrings();
	lines.add (juce::String (juce::Time::currentTimeMillis()) + "\t" + journalKey (input, folder, track));
	if (lines.size() > journalLines)
		lines.removeRange (0, lines.size() - journalLines);
	journal.replaceWithText (lines.joinIntoString ("\n") + "\n");
}

bool StemCreatorLock::doneSince (const juce::File& input, const juce::String& folder, const juce::String& track, juce::int64 sinceMs) const
{
	juce::StringArray lines;
	lines.addLines (libraryFolder.getChildFile ("stemcreator.done").loadFileAsString());
	const auto key = journalKey (input, folder, track);
	for (const auto& line : lines)
	{
		const auto tab = line.indexOfChar ('\t');
		if (tab > 0 && line.substring (tab + 1) == key && line.substring (0, tab).getLargeIntValue() >= sinceMs)
			return true;
	}
	return false;
}
