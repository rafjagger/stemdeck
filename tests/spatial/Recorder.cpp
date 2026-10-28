// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "Recorder.h"

#include <chrono>
#include <thread>

namespace
{
constexpr double rate = 44100.0;

// Feeds the recorder like the audio thread does: noise, in blocks.
void feed (Recorder& recorder, int blocks)
{
	juce::Random random (7);
	std::vector<float> left (512), right (512);
	for (int b = 0; b < blocks; ++b)
	{
		for (size_t i = 0; i < left.size(); ++i)
			left[i] = right[i] = random.nextFloat() * 2.0f - 1.0f;
		const float* channels[2] = { left.data(), right.data() };
		recorder.inputBlock (channels, 2, (int) left.size());
		std::this_thread::sleep_for (std::chrono::milliseconds (1));
	}
}

// Polls like the message thread until the recorder stops itself, or gives up.
bool stopsItself (Recorder& recorder, Recorder& feeder)
{
	for (int attempt = 0; attempt < 400; ++attempt)
	{
		feed (feeder, 4);
		if (recorder.poll())
			return true;
	}
	return false;
}

// A file that takes `capacity` bytes and then fails like a pulled stick.
class FailingStream : public juce::OutputStream
{
public:
	FailingStream (const juce::File& file, juce::int64 capacityBytes) : inner (file), capacity (capacityBytes) {}
	bool write (const void* data, size_t number) override
	{
		if (inner.getPosition() + (juce::int64) number > capacity)
			return false;
		return inner.write (data, number);
	}
	void flush() override { inner.flush(); }
	bool setPosition (juce::int64 newPosition) override { return inner.setPosition (newPosition); }
	juce::int64 getPosition() override { return inner.getPosition(); }

private:
	juce::FileOutputStream inner;
	juce::int64 capacity;
};

// What a player gets from the take afterwards: its samples, decoded.
struct Playback { juce::int64 length = 0; float peak = 0.0f; };

Playback playbackOf (const juce::File& file)
{
	juce::FlacAudioFormat flac;
	std::unique_ptr<juce::AudioFormatReader> reader (flac.createReaderFor (file.createInputStream().release(), true));
	if (reader == nullptr)
		return {};

	// The length is the header's: a take cut short must still have a true one.
	Playback result;
	result.length = reader->lengthInSamples;
	juce::AudioBuffer<float> buffer (2, (int) std::min<juce::int64> (result.length, 44100 * 60));
	if (buffer.getNumSamples() > 0 && reader->read (&buffer, 0, buffer.getNumSamples(), 0, true, true))
		result.peak = buffer.getMagnitude (0, buffer.getNumSamples());
	return result;
}

juce::File freshFolder (const char* name)
{
	auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (juce::String ("stemdeck-Recorder-") + name);
	folder.deleteRecursively();
	folder.createDirectory();
	return folder;
}
}

TEST (Recorder, StopsALocalTakeAtTheLimit)
{
	juce::ScopedJuceInitialiser_GUI init;
	const auto folder = freshFolder ("limit");
	folder.getChildFile ("earlier.flac").replaceWithText (juce::String::repeatedString ("x", 1000));

	Recorder recorder;
	Recorder::TakeRules rules;
	rules.localLimitBytes = 1000 + 20000;   // what is there plus 20 kB of take
	ASSERT_TRUE (recorder.start (folder, rate, rules).isEmpty());

	EXPECT_TRUE (stopsItself (recorder, recorder));
	EXPECT_FALSE (recorder.isRecording());
	EXPECT_TRUE (recorder.stopReason().containsIgnoreCase ("100 GB") || recorder.stopReason().containsIgnoreCase ("limit"))
		<< recorder.stopReason();
	EXPECT_TRUE (recorder.getFile().existsAsFile()) << "what was written stays";
	const auto playback = playbackOf (recorder.getFile());
	EXPECT_GT (playback.length, 0) << "a playable FLAC";
	EXPECT_GT (playback.peak, 0.1f) << "with the noise in it";
	EXPECT_EQ (recorder.stopCause(), Recorder::StopCause::limitReached);
	EXPECT_TRUE (folder.getChildFile ("earlier.flac").existsAsFile()) << "nothing is deleted";
	folder.deleteRecursively();
}

TEST (Recorder, DoesNotStartLocallyAtTheLimit)
{
	juce::ScopedJuceInitialiser_GUI init;
	const auto folder = freshFolder ("full");
	folder.getChildFile ("earlier.flac").replaceWithText (juce::String::repeatedString ("x", 1000));

	Recorder recorder;
	Recorder::TakeRules rules;
	rules.localLimitBytes = 1000;
	const auto error = recorder.start (folder, rate, rules);
	EXPECT_TRUE (error.isNotEmpty());
	EXPECT_FALSE (recorder.isRecording());
	EXPECT_EQ (folder.getNumberOfChildFiles (juce::File::findFiles), 1) << "no empty take left behind";
	folder.deleteRecursively();
}

TEST (Recorder, AWriteErrorStopsTheTakeAndSaysSo)
{
	juce::ScopedJuceInitialiser_GUI init;
	const auto folder = freshFolder ("failing");
	const auto take = folder.getChildFile ("take.flac");
	Recorder recorder;
	ASSERT_TRUE (recorder.startOn (std::make_unique<FailingStream> (take, 30000), take, rate, {}).isEmpty());

	EXPECT_TRUE (stopsItself (recorder, recorder));
	EXPECT_FALSE (recorder.isRecording());
	EXPECT_TRUE (recorder.stopReason().isNotEmpty());
	EXPECT_EQ (recorder.stopCause(), Recorder::StopCause::writeFailed);

	const auto playback = playbackOf (take);
	EXPECT_GT (playback.length, 0) << "what reached the disk is a playable FLAC";
	EXPECT_GT (playback.peak, 0.1f);
	folder.deleteRecursively();
}

TEST (Recorder, AVanishedTargetStopsTheTake)
{
	juce::ScopedJuceInitialiser_GUI init;
	const auto folder = freshFolder ("vanish");
	const auto take = folder.getChildFile ("take.flac");
	Recorder recorder;
	std::atomic<bool> there { true };
	Recorder::TakeRules rules;
	rules.targetStillThere = [&there] { return there.load(); };
	ASSERT_TRUE (recorder.startOn (std::make_unique<FailingStream> (take, 1 << 30), take, rate, rules).isEmpty());

	feed (recorder, 4);
	EXPECT_FALSE (recorder.poll());
	EXPECT_TRUE (recorder.isRecording());
	there = false;
	EXPECT_TRUE (recorder.poll());
	EXPECT_FALSE (recorder.isRecording());
	EXPECT_TRUE (recorder.stopReason().containsIgnoreCase ("removed"));
	EXPECT_EQ (recorder.stopCause(), Recorder::StopCause::targetGone);
	folder.deleteRecursively();
}

TEST (Recorder, AnOrdinaryStopHasNoReason)
{
	juce::ScopedJuceInitialiser_GUI init;
	const auto folder = freshFolder ("plain");
	Recorder recorder;
	ASSERT_TRUE (recorder.start (folder, rate).isEmpty());
	feed (recorder, 4);
	recorder.stop();
	EXPECT_TRUE (recorder.stopReason().isEmpty());
	folder.deleteRecursively();
}
