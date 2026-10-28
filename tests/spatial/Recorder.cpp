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

// A stream that takes `capacity` bytes and then fails like a pulled stick.
class FailingStream : public juce::OutputStream
{
public:
	explicit FailingStream (juce::int64 capacityBytes) : capacity (capacityBytes) {}
	bool write (const void*, size_t number) override
	{
		if (position + (juce::int64) number > capacity)
			return false;
		position += (juce::int64) number;
		return true;
	}
	void flush() override {}
	bool setPosition (juce::int64 newPosition) override { position = newPosition; return true; }
	juce::int64 getPosition() override { return position; }

private:
	juce::int64 capacity, position = 0;
};

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
	Recorder recorder;
	ASSERT_TRUE (recorder.startOn (std::make_unique<FailingStream> (30000), juce::File ("/nonexistent/take.flac"), rate, {}).isEmpty());

	EXPECT_TRUE (stopsItself (recorder, recorder));
	EXPECT_FALSE (recorder.isRecording());
	EXPECT_TRUE (recorder.stopReason().isNotEmpty());
}

TEST (Recorder, AVanishedTargetStopsTheTake)
{
	juce::ScopedJuceInitialiser_GUI init;
	Recorder recorder;
	std::atomic<bool> there { true };
	Recorder::TakeRules rules;
	rules.targetStillThere = [&there] { return there.load(); };
	ASSERT_TRUE (recorder.startOn (std::make_unique<FailingStream> (1 << 30), juce::File ("/nonexistent/take.flac"), rate, rules).isEmpty());

	feed (recorder, 4);
	EXPECT_FALSE (recorder.poll());
	EXPECT_TRUE (recorder.isRecording());
	there = false;
	EXPECT_TRUE (recorder.poll());
	EXPECT_FALSE (recorder.isRecording());
	EXPECT_TRUE (recorder.stopReason().containsIgnoreCase ("removed"));
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
