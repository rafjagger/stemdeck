#pragma once

#include <JuceHeader.h>
#include "StemJob.h"

#include <atomic>
#include <mutex>

// Turns stereo files into stem sets in the library, one at a time, in the
// background: ffmpeg decodes any format to one WAV, Demucs separates it into
// drums, bass, other and vocals, and the four stems are moved into
// <library>/<Artist>/<Album>/ with the original copied to originals/ (the
// rules are in StemJob.h). Nothing lands in the library before a job is
// complete: the work happens in ~/.cache/StemDeck/jobs/<id>/.
//
// Each command runs as its own process group, so pausing (while a deck
// plays), cancelling and quitting reach everything it started.
class StemCreator : private juce::Thread
{
public:
	StemCreator();
	~StemCreator() override;

	void setLibraryFolder (const juce::File& folder);
	// Where the separator's venv lives (tools/setup-separator.sh makes it).
	void setVenv (const juce::File& venv);
	void setFast (bool allCores) { fast = allCores; }

	int add (const juce::File& input, const juce::String& artist, const juce::String& album, const juce::String& track);
	void cancel (int id);
	void cancelRunning();

	// While a deck plays or StemDeck is the tempo master: stopped, not killed.
	void setPaused (bool shouldPause);

	struct Status
	{
		bool running = false, paused = false;
		juce::String track;
		double progress = 0.0;  // of the separation
		int waiting = 0;
		juce::String lastError;
	};
	Status status() const;

	// On the message thread, with the first stem of the new set.
	std::function<void (const juce::File& firstStem)> onSetCreated;

private:
	void run() override;
	bool runJob (const StemJobEntry& job, juce::String& error);
	bool execute (const std::vector<std::string>& argv, const std::vector<std::string>& environment,
				  const std::function<void (const std::string&)>& onLine, juce::String& error);
	void signalChild (int signal);

	mutable std::mutex lock;
	StemJobQueue queue;
	juce::File libraryFolder, venvFolder;
	juce::String runningTrack, lastError;
	double progress = 0.0;
	std::atomic<bool> fast { false }, paused { false }, cancelRequested { false };
	std::atomic<int> childGroup { -1 };
	juce::WaitableEvent wake;
};
