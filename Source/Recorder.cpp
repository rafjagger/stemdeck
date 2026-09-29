#include "Recorder.h"
#include "RecordingName.h"

Recorder::Recorder()
{
	writerThread.startThread();
}

Recorder::~Recorder()
{
	stop();
	writerThread.stopThread (2000);
}

juce::String Recorder::start (const juce::File& folder, double sampleRate)
{
	stop();

	if (! folder.createDirectory())
		return "Cannot create the folder: " + folder.getFullPathName();

	const auto now = juce::Time::getCurrentTime();
	auto target = folder.getChildFile (recordingFileName (now.getYear(), now.getMonth() + 1, now.getDayOfMonth(),
														  now.getHours(), now.getMinutes(), now.getSeconds()));
	target = target.getNonexistentSibling();

	std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (target);
	if (static_cast<juce::FileOutputStream*> (stream.get())->failedToOpen())
		return "Cannot write the file: " + target.getFullPathName();

	juce::FlacAudioFormat flac;
	auto flacWriter = flac.createWriterFor (stream, juce::AudioFormatWriterOptions{}
														.withSampleRate (sampleRate)
														.withNumChannels (2)
														.withBitsPerSample (24));
	if (flacWriter == nullptr)
		return "Cannot create the FLAC writer";

	// Ten seconds of FIFO: room for a slow disk moment.
	auto threaded = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (flacWriter.release(), writerThread, (int) (sampleRate * 10));

	const juce::SpinLock::ScopedLockType sl (writerLock);
	writer = std::move (threaded);
	file = target;
	rate = sampleRate;
	samplesWritten = 0;
	dropped = false;
	recording = true;
	return {};
}

void Recorder::stop()
{
	std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> finishing;
	{
		const juce::SpinLock::ScopedLockType sl (writerLock);
		recording = false;
		finishing = std::move (writer);
	}
	finishing.reset();   // flushes the FIFO and closes the file
}

double Recorder::getSeconds() const
{
	return (double) samplesWritten.load() / rate.load();
}

void Recorder::inputBlock (const float* const* channels, int numChannels, int numSamples)
{
	for (int c = 0; c < juce::jmin (2, numChannels); ++c)
	{
		const auto range = juce::FloatVectorOperations::findMinAndMax (channels[c], numSamples);
		const auto peak = juce::jmax (std::abs (range.getStart()), std::abs (range.getEnd()));
		if (peak > peaks[(size_t) c].load())
			peaks[(size_t) c] = peak;
	}

	if (! recording.load() || numChannels < 2)
		return;

	const juce::SpinLock::ScopedTryLockType sl (writerLock);
	if (! sl.isLocked() || writer == nullptr)
		return;

	if (writer->write (channels, numSamples))
		samplesWritten += numSamples;
	else
		dropped = true;
}
