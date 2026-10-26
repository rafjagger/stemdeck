// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "StemCreator.h"
#include "StemCreatorLock.h"

// Two -bformat instances share the library folder, and both may hold the
// same queued job (each imported the two-deck session's queue). A lock file
// in the library folder lets one separate at a time; a job the other one
// finished meanwhile is not run again (Review Focus 3).
namespace
{
template <typename Done>
bool waitFor (Done done, int timeoutMs = 10000)
{
	const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
	while (! done())
	{
		if (juce::Time::getMillisecondCounter() > end)
			return false;
		juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
	}
	return true;
}

struct Library
{
	Library()
	{
		folder.createDirectory();
		input.replaceWithText ("not audio: the job fails at once without a separator");
	}
	~Library() { folder.deleteRecursively(); }

	juce::File folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("stemdeck-creatorlock", "");
	juce::File input = folder.getSiblingFile (folder.getFileName() + "-track.wav");
};
}

TEST (StemCreatorLock, OneHolderAtATime)
{
	Library library;
	StemCreatorLock first (library.folder), second (library.folder);
	EXPECT_EQ (first.tryAcquire(), StemCreatorLock::Result::acquired);
	EXPECT_EQ (second.tryAcquire(), StemCreatorLock::Result::heldElsewhere);
	EXPECT_TRUE (library.folder.getChildFile ("stemcreator.lock").existsAsFile());
	first.release();
	EXPECT_EQ (second.tryAcquire(), StemCreatorLock::Result::acquired);
	EXPECT_EQ (first.tryAcquire(), StemCreatorLock::Result::heldElsewhere);
}

TEST (StemCreatorLock, SecondInstanceWaits)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	Library library;
	StemCreatorLock other (library.folder);   // the other instance, separating
	ASSERT_EQ (other.tryAcquire(), StemCreatorLock::Result::acquired);

	{
		StemCreator creator;
		creator.setVenv (library.folder.getChildFile ("no-separator"));
		creator.setLibraryFolder (library.folder);
		creator.add (library.input, "", "track");

		ASSERT_TRUE (waitFor ([&] { return creator.status().waitingForOtherInstance; })) << "it says it waits";
		juce::Thread::sleep (1500);
		auto status = creator.status();
		EXPECT_FALSE (status.running) << "and starts nothing while the other one separates";
		EXPECT_EQ (status.waiting, 1);
		EXPECT_TRUE (status.lastError.isEmpty());

		other.release();
		ASSERT_TRUE (waitFor ([&] { return creator.status().lastError.isNotEmpty(); })) << "then it runs the job";
		status = creator.status();
		EXPECT_FALSE (status.waitingForOtherInstance);
		EXPECT_EQ (status.waiting, 0);
		EXPECT_TRUE (status.lastError.contains ("separator")) << status.lastError;
		EXPECT_EQ (other.tryAcquire(), StemCreatorLock::Result::acquired) << "and lets go of the lock after it";
	}
}

TEST (StemCreatorLock, AJobTheOtherInstanceFinishedIsNotRunAgain)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	Library library;
	StemCreatorLock other (library.folder);
	ASSERT_EQ (other.tryAcquire(), StemCreatorLock::Result::acquired);

	StemCreator creator;
	creator.setVenv (library.folder.getChildFile ("no-separator"));
	creator.setLibraryFolder (library.folder);
	creator.add (library.input, "Artist", "track");
	ASSERT_TRUE (waitFor ([&] { return creator.status().waitingForOtherInstance; }));

	juce::Thread::sleep (20);
	other.recordDone (library.input, "Artist", "track");   // the same job, done there
	other.release();

	ASSERT_TRUE (waitFor ([&] { return creator.pendingJobs().empty(); })) << "taken off the queue";
	juce::Thread::sleep (300);
	EXPECT_TRUE (creator.status().lastError.isEmpty()) << "without running it: " << creator.status().lastError;

	// The same file queued again later is a new wish, and runs.
	creator.add (library.input, "Artist", "track");
	ASSERT_TRUE (waitFor ([&] { return creator.status().lastError.isNotEmpty(); }));
}
