#pragma once

#include <JuceHeader.h>
#include <sys/types.h>
#include <thread>

// Runs tools/screencast.sh -- a remote screen and its sound streamed here and
// recorded to recordings/ -- as a child StemDeck can start and stop. The
// script hands its picture over as raw frames (PREVIEW=WxH) on one pipe and
// its sound, for the meters only, on another (8 kHz stereo on fd 3); both
// are read here on threads of their own. Nothing is played here, so the
// audio graph is never touched (a new output in it ends zita-j2n).
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

	// The sound's peak since the last call, 0..1, channel 0 or 1.
	float popPeak (int channel) { return peaks[(size_t) channel].exchange (0.0f); }

private:
	void run() override;

	void readSound();

	pid_t child = -1;
	int pipeFd = -1, soundFd = -1;
	std::thread soundReader;
	std::array<std::atomic<float>, 2> peaks {};
	double startedAt = 0.0;
	juce::CriticalSection frameLock;
	juce::Image frame;
	std::atomic<int> frames { 0 };
};
