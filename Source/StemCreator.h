#pragma once

#include <JuceHeader.h>
#include "StemJob.h"
#include "ProcessGroup.h"

#include <atomic>
#include <mutex>

// Turns stereo files into stem sets in the library, one at a time, in the
// background: ffmpeg decodes any format to one WAV, Demucs separates it into
// drums, bass, other and vocals, and the four stems are moved into
// <library>/<Artist>/<Album>/ with the original copied to originals/ (the
// rules are in StemJob.h). Nothing lands in the library before a job is
// complete: the work happens in ~/.cache/StemDeck/jobs/<id>/.
//
// The commands run through ProcessGroup, so cancelling and quitting reach
// everything they started. They run beside the decks, never on the audio CPU.
class StemCreator : private juce::Thread
{
public:
	StemCreator();
	~StemCreator() override;

	void setLibraryFolder (const juce::File& folder);
	// Where the separator's venv lives (tools/setup-separator.sh makes it).
	void setVenv (const juce::File& venv);
	// How many CPU cores Demucs may use -- never the audio CPU (StemJob.h);
	// 0, the default, is all the others.
	void setMaxCores (int count) { maxCores = juce::jmax (0, count); }
	// Where the separator should be, and whether it is there.
	juce::File venv() const;
	bool isInstalled() const { return venv().getChildFile ("bin/demucs").existsAsFile(); }

	// `folder`: the target, relative to the library (StemJob.h).
	int add (const juce::File& input, const juce::String& folder, const juce::String& track);
	void cancel (int id);
	void cancelRunning();

	struct Status
	{
		bool running = false;
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
	bool stopRequested() const { return cancelRequested || threadShouldExit(); }

	mutable std::mutex lock;
	StemJobQueue queue;
	juce::File libraryFolder, venvFolder;
	juce::String runningTrack, lastError;
	double progress = 0.0;
	std::atomic<int> maxCores { 0 };
	std::atomic<bool> cancelRequested { false };
	ProcessGroup processes;
	juce::WaitableEvent wake;
};
