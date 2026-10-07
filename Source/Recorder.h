#pragma once

#include <JuceHeader.h>
#include "JackOutput.h"
#include <cstdlib>
#include "DataPaths.h"

// Records StemDeck's two JACK inputs (rec_L, rec_R) to a 24-bit FLAC file.
// The audio thread only hands blocks to a FIFO; a background thread writes
// them, so a slow disk never costs a dropout. Input levels are metered
// whether it records or not, to see that something is connected.
class Recorder : public JackOutput::InputSink
{
public:
	Recorder();
	~Recorder() override;

	// ~/.local/share/stemdeck/recordings ($XDG_DATA_HOME honoured), wherever
	// StemDeck was started from: the package starts it in its data folder, a
	// dev build runs from a checkout, and both record to the same place
	// (2026-10-07; before, recordings/ grew inside the checkout).
	static juce::File defaultFolder()
	{
		const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName().toStdString();
		return juce::File (juce::String::fromUTF8 (recordingsFolder (home, std::getenv ("XDG_DATA_HOME")).c_str()));
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
