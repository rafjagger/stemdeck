#include "Recorder.h"
#include "RecordingName.h"
#include "RecordTarget.h"

namespace
{
// Notices what the writer thread cannot say: a write that failed (disk full,
// stick pulled) and how many bytes went out.
class WatchedStream : public juce::OutputStream
{
public:
	WatchedStream (std::unique_ptr<juce::OutputStream> inner_, std::function<void (bool, juce::int64)> report_)
		: inner (std::move (inner_)), report (std::move (report_)) {}

	bool write (const void* data, size_t number) override
	{
		const auto ok = inner->write (data, number);
		report (ok, ok ? (juce::int64) number : 0);
		return ok;
	}
	void flush() override { inner->flush(); }
	bool setPosition (juce::int64 position) override { return inner->setPosition (position); }
	juce::int64 getPosition() override { return inner->getPosition(); }

private:
	std::unique_ptr<juce::OutputStream> inner;
	std::function<void (bool, juce::int64)> report;
};
}

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
	return start (folder, sampleRate, TakeRules{});
}

juce::String Recorder::start (const juce::File& folder, double sampleRate, TakeRules newRules)
{
	stop();

	juce::int64 base = 0;
	if (newRules.localLimitBytes > 0)
	{
		base = RecordTarget::folderBytes (folder.getFullPathName().toStdString());
		if (! RecordTarget::localHasRoom (base, newRules.localLimitBytes))
			return "The local recordings folder holds its limit of " + juce::String (newRules.localLimitBytes / 1'000'000'000)
				   + " GB: nothing was deleted, free some space or plug in a USB stick";
	}

	if (! folder.createDirectory())
		return "Cannot create the folder: " + folder.getFullPathName();

	const auto now = juce::Time::getCurrentTime();
	auto target = folder.getChildFile (recordingFileName (now.getYear(), now.getMonth() + 1, now.getDayOfMonth(),
														  now.getHours(), now.getMinutes(), now.getSeconds()));
	target = target.getNonexistentSibling();

	auto stream = std::make_unique<juce::FileOutputStream> (target);
	if (stream->failedToOpen())
		return "Cannot write the file: " + target.getFullPathName();

	folderBytesAtStart = base;
	return startOn (std::move (stream), target, sampleRate, std::move (newRules));
}

juce::String Recorder::startOn (std::unique_ptr<juce::OutputStream> stream, const juce::File& target, double sampleRate, TakeRules newRules)
{
	stop();
	if (newRules.localLimitBytes <= 0)
		folderBytesAtStart = 0;

	auto newWatch = std::make_shared<WriteWatch>();
	std::unique_ptr<juce::OutputStream> watched = std::make_unique<WatchedStream> (std::move (stream),
		[newWatch] (bool ok, juce::int64 bytes)
		{
			if (! ok)
				newWatch->failed = true;
			newWatch->bytes += bytes;
		});

	juce::FlacAudioFormat flac;
	auto flacWriter = flac.createWriterFor (watched, juce::AudioFormatWriterOptions{}
														.withSampleRate (sampleRate)
														.withNumChannels (2)
														.withBitsPerSample (24));
	if (flacWriter == nullptr)
		return "Cannot create the FLAC writer";

	// Ten seconds of FIFO: room for a slow disk moment.
	auto threaded = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (flacWriter.release(), writerThread, (int) (sampleRate * 10));

	const juce::SpinLock::ScopedLockType sl (writerLock);
	writer = std::move (threaded);
	watch = std::move (newWatch);
	rules = std::move (newRules);
	file = target;
	reason = {};
	cause = StopCause::none;
	rate = sampleRate;
	samplesWritten = 0;
	dropped = false;
	recording = true;
	return {};
}

bool Recorder::poll()
{
	if (! recording.load())
		return false;

	StopCause why = StopCause::none;
	juce::String text;
	if (watch->failed.load())
	{
		why = StopCause::writeFailed;
		text = "The disk stopped taking data (full, or the stick was pulled): the take ends here";
	}
	else if (rules.localLimitBytes > 0 && ! RecordTarget::localHasRoom (folderBytesAtStart + watch->bytes.load(), rules.localLimitBytes))
	{
		why = StopCause::limitReached;
		text = "The local recordings folder reached its limit of " + juce::String (rules.localLimitBytes / 1'000'000'000)
			   + " GB: the take ends here, nothing was deleted";
	}
	else if (rules.targetStillThere && ! rules.targetStillThere())
	{
		why = StopCause::targetGone;
		text = "The USB stick was removed: the take ends here";
	}
	else
		return false;

	stop();
	reason = text;
	cause = why;
	return true;
}

void Recorder::stop()
{
	std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> finishing;
	{
		const juce::SpinLock::ScopedLockType sl (writerLock);
		recording = false;
		reason = {};
		cause = StopCause::none;
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
