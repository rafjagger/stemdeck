// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

// An exclusive lock between processes on a lock file (flock): let go on
// release, on destruction, and with the process if it dies. Two objects in
// one process on the same file exclude each other too.
class FileLock
{
public:
	explicit FileLock (const juce::File& lockFile);
	~FileLock();

	// unavailable: the lock file cannot be opened (no folder, no rights).
	enum class Result { acquired, heldElsewhere, unavailable };
	Result tryAcquire();   // never waits
	Result acquire();      // waits for the holder
	void release();

private:
	Result take (bool wait);

	juce::File file;
	int descriptor = -1;

	JUCE_DECLARE_NON_COPYABLE (FileLock)
};
