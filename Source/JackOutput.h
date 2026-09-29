#pragma once

#include <JuceHeader.h>
#include <jack/jack.h>

// Minimal JACK client with a fixed set of named output ports.
// JUCE's own JACK device mirrors the port count of the client it connects to
// (2 for a stereo card), so it cannot expose 4 x stereo; this one always does.
class JackOutput
{
public:
	JackOutput() = default;
	~JackOutput();

	// Opens the client, registers the ports and starts pulling audio from
	// `source`. Returns an error message, or an empty string on success.
	// Never starts a JACK server on its own, and never connects the ports:
	// routing is left to the user (qjackctl, patchbay...).
	juce::String open (const juce::String& clientName, const juce::StringArray& portNames, juce::AudioSource& source);
	void close();

	bool isRunning() const { return client != nullptr && ! serverShutDown.load(); }
	juce::String getClientName() const { return clientName; }
	double getSampleRate() const { return sampleRate.load(); }
	int getBufferSize() const { return (int) bufferSize.load(); }
	int getNumPorts() const { return (int) ports.size(); }
	int getNumConnectedPorts() const;
	int getXrunCount() const { return xruns.load(); }

private:
	static int processCallback (jack_nframes_t numFrames, void* arg);
	static int bufferSizeCallback (jack_nframes_t numFrames, void* arg);
	static int sampleRateCallback (jack_nframes_t rate, void* arg);
	static int xrunCallback (void* arg);
	static void shutdownCallback (void* arg);

	jack_client_t* client = nullptr;
	std::vector<jack_port_t*> ports;
	std::vector<float*> channelPointers; // sized once, filled in the process callback
	juce::AudioSource* source = nullptr;
	juce::String clientName;

	std::atomic<double> sampleRate { 0.0 };
	std::atomic<jack_nframes_t> bufferSize { 0 };
	std::atomic<int> xruns { 0 };
	std::atomic<bool> serverShutDown { false };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JackOutput)
};
