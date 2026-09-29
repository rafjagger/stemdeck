#pragma once

#include <JuceHeader.h>
#include "JackOutput.h"

// Records StemDeck's two JACK inputs (rec_L, rec_R) to a 24-bit FLAC file.
// The audio thread only hands blocks to a FIFO; a background thread writes
// them, so a slow disk never costs a dropout. Input levels are metered
// whether it records or not, to see that something is connected.
class Recorder : public JackOutput::InputSink
{
public:
	Recorder();
	~Recorder() override;

	// recordings/ in the program's folder (where start.sh runs it from, like
	// stems/): moves with the checkout.
	static juce::File defaultFolder()
	{
		return juce::File::getCurrentWorkingDirectory().getChildFile ("recordings");
	}

	// Starts a new file in `folder`, named by the time. Returns an error, or
	// an empty string.
	juce::String start (const juce::File& folder, double sampleRate);
	void stop();   // the file is complete once this returns

	bool isRecording() const { return recording.load(); }
	juce::File getFile() const { return file; }
	double getSeconds() const;
	bool hasDropped() const { return dropped.load(); }   // the disk could not keep up at some point

	float popPeak (int channel) { return peaks[(size_t) channel].exchange (0.0f); }

	// JackOutput::InputSink, on the audio thread.
	void inputBlock (const float* const* channels, int numChannels, int numSamples) override;

private:
	juce::TimeSliceThread writerThread { "Recorder" };
	juce::SpinLock writerLock;   // the audio thread only tries it: never waits
	std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
	juce::File file;
	std::atomic<bool> recording { false }, dropped { false };
	std::atomic<juce::int64> samplesWritten { 0 };
	std::atomic<double> rate { 44100.0 };
	std::array<std::atomic<float>, 2> peaks {};
};
