// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include "FileLock.h"

// One-deck StemDeck instances (-bformat --deck A, B, ...) share the library folder
// and may hold the same queued job: each imported the two-deck session's
// queue. One separates at a time: whoever holds <library>/stemcreator.lock
// (flock, so a crashed instance lets go with its process). A job done is
// written to <library>/stemcreator.done, so the other one does not run the
// same job again once it gets the lock.
class StemCreatorLock
{
public:
	explicit StemCreatorLock (const juce::File& libraryFolder);
	~StemCreatorLock();

	// unavailable: the lock file cannot be made (no folder) -- run without it.
	using Result = FileLock::Result;
	Result tryAcquire();
	void release() { lock.release(); }

	// Held: the job's set went into the library just now.
	void recordDone (const juce::File& input, const juce::String& folder, const juce::String& track);
	// Done by anyone at or after `sinceMs` (juce::Time::currentTimeMillis).
	bool doneSince (const juce::File& input, const juce::String& folder, const juce::String& track, juce::int64 sinceMs) const;

private:
	juce::File libraryFolder;
	FileLock lock;

	JUCE_DECLARE_NON_COPYABLE (StemCreatorLock)
};
