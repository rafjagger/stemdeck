// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "RecorderControls.h"

namespace
{
juce::Image paintButton (RecorderControls& controls)
{
	controls.button.setBounds (0, 0, 220, 60);
	return controls.button.createComponentSnapshot (controls.button.getLocalBounds());
}

int differingPixels (const juce::Image& a, const juce::Image& b)
{
	const juce::Image::BitmapData first (a, juce::Image::BitmapData::readOnly), second (b, juce::Image::BitmapData::readOnly);
	int differing = 0;
	for (int y = 0; y < a.getHeight(); ++y)
		for (int x = 0; x < a.getWidth(); ++x)
			differing += first.getPixelColour (x, y) != second.getPixelColour (x, y) ? 1 : 0;
	return differing;
}

int distinctColours (const juce::Image& image)
{
	const juce::Image::BitmapData pixels (image, juce::Image::BitmapData::readOnly);
	std::set<juce::uint32> colours;
	for (int y = 0; y < image.getHeight(); ++y)
		for (int x = 0; x < image.getWidth(); ++x)
			colours.insert (pixels.getPixelColour (x, y).getARGB());
	return (int) colours.size();
}
}

// A page test must paint: every face the rec button wears, painted, and each
// one a different picture from the others.
TEST (RecorderControls, TheButtonShowsTheTargetInEveryFace)
{
	juce::ScopedJuceInitialiser_GUI init;
	Recorder recorder;
	bool stickThere = true;
	RecorderControls controls (recorder);
	controls.setTargets ("/nonexistent/a3-usb", "/nonexistent/local", [&stickThere] { return stickThere; });

	controls.update ("stemdeck");
	EXPECT_TRUE (controls.button.getButtonText().contains ("USB")) << controls.button.getButtonText();
	const auto usb = paintButton (controls);

	stickThere = false;
	controls.refreshTarget();
	controls.update ("stemdeck");
	EXPECT_TRUE (controls.button.getButtonText().contains ("LOCAL")) << controls.button.getButtonText();
	const auto local = paintButton (controls);

	controls.showStopped ("USB stick removed: the take stops here");
	controls.update ("stemdeck");
	EXPECT_TRUE (controls.button.getButtonText().contains ("!")) << controls.button.getButtonText();
	EXPECT_TRUE (controls.button.getTooltip().contains ("removed"));
	const auto stopped = paintButton (controls);

	for (const auto* image : { &usb, &local, &stopped })
		EXPECT_GT (distinctColours (*image), 2) << "the face painted more than a ground";
	EXPECT_GT (differingPixels (usb, local), 20);
	EXPECT_GT (differingPixels (local, stopped), 20);
	EXPECT_GT (differingPixels (usb, stopped), 20);
}

TEST (RecorderControls, TheTargetShowsDuringATakeToo)
{
	juce::ScopedJuceInitialiser_GUI init;
	Recorder recorder;
	RecorderControls controls (recorder);
	const auto stick = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("stemdeck-RC-stick");
	controls.setTargets (stick.getFullPathName().toStdString(), "/nonexistent/local", [] { return true; });

	controls.toggle (44100.0);
	ASSERT_TRUE (recorder.isRecording());
	EXPECT_TRUE (controls.button.getButtonText().contains ("USB"));
	const auto during = paintButton (controls);
	EXPECT_GT (distinctColours (during), 2);
	controls.toggle (44100.0);
	stick.deleteRecursively();
}
