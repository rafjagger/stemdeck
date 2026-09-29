#pragma once

#include <JuceHeader.h>
#include <sys/types.h>

// Runs tools/screencast.sh -- a remote screen and its sound streamed here,
// recorded to recordings/ and shown live -- as a child StemDeck can start
// and stop. The script runs in a process group of its own; stop() sends it
// what Ctrl+C would (SIGINT to the group): ssh ends, the remote ffmpeg ends
// with it, and the script turns the recording into an .mp4.
class Screencast
{
public:
	~Screencast();

	// Returns an error, or an empty string.
	juce::String start (const juce::File& script, const juce::String& host, bool liveView);
	void stop();

	// Polls the child; false once it has finished (after stop(), or on its own).
	bool isRunning();
	double getSeconds() const;

private:
	pid_t child = -1;
	double startedAt = 0.0;
};
