#pragma once

#include <JuceHeader.h>
#include <sys/types.h>

// Runs tools/screencast.sh -- a remote screen and its sound streamed here and
// recorded to recordings/ -- as a child StemDeck can start and stop. The
// script hands its picture over as raw frames (PREVIEW=WxH) on a pipe, read
// here on a thread of its own; nothing is played here, so the audio graph is
// never touched (a new output in it ends zita-j2n).
//
// The script runs in a process group of its own; stop() sends it what Ctrl+C
// would (SIGINT to the group): ssh ends, the remote ffmpeg ends with it, and
// the script turns the recording into an .mp4.
class Screencast : private juce::Thread
{
public:
	static constexpr int frameWidth = 240, frameHeight = 320;   // portrait, like the Motion screen

	Screencast() : juce::Thread ("Screencast frames") {}
	~Screencast() override;

	// Returns an error, or an empty string.
	juce::String start (const juce::File& script, const juce::String& host);
	void stop();

	// Polls the child; false once it has finished (after stop(), or on its own).
	bool isRunning();
	double getSeconds() const;

	// The latest frame (invalid before the first), and how many came so far.
	juce::Image getFrame() const;
	int getFrameCount() const { return frames.load(); }

private:
	void run() override;

	pid_t child = -1;
	int pipeFd = -1;
	double startedAt = 0.0;
	juce::CriticalSection frameLock;
	juce::Image frame;
	std::atomic<int> frames { 0 };
};
