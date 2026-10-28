// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "RecorderControls.h"

#include <set>

namespace
{
// The rec button's slot on the rig: 9 % of a 1280-wide bar (OneDeckComponent::resized),
// as high as the bar (MainComponent gives it less width, never less height).
constexpr int slotWidth = 115, slotHeight = 44;

using Stick = RecordTarget::Stick;

juce::Image paintButton (RecorderControls& controls)
{
	controls.button.setBounds (0, 0, slotWidth, slotHeight);
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

// The text must not be shrunk below what JUCE still draws whole: the button's
// own font at the slot's size has to fit, so no ellipsis eats the last glyph.
bool fits (const juce::TextButton& button)
{
	const auto font = button.getLookAndFeel().getTextButtonFont (const_cast<juce::TextButton&> (button), slotHeight);
	juce::GlyphArrangement glyphs;
	glyphs.addLineOfText (font, button.getButtonText(), 0.0f, 0.0f);
	const auto margin = juce::jmin (slotHeight / 2, 2 + slotHeight / 4) * 2;   // drawButtonText's indent, both sides
	return glyphs.getBoundingBox (0, -1, false).getWidth() <= (float) (slotWidth - margin);
}

// A stick the tests control; "write-probe" answers what a real access() would.
struct Stage
{
	Stick stick = Stick::writable;
	bool writable = true;
	juce::uint32 now = 1000;
	juce::File mount = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("stemdeck-RC-stick");

	void attach (RecorderControls& controls)
	{
		controls.setClock ([this] { return now; });
		controls.setTargets (mount.getFullPathName().toStdString(), mount.getSiblingFile ("stemdeck-RC-local").getFullPathName().toStdString(),
							 { [this] { return stick; }, [this] { return writable; } });
	}
	~Stage()
	{
		mount.deleteRecursively();
		mount.getSiblingFile ("stemdeck-RC-local").deleteRecursively();
	}
};
}

// A page test must paint: every face the rec button wears, painted at its
// real slot width, each one a different picture from the others.
TEST (RecorderControls, TheButtonShowsTheTargetInEveryFace)
{
	juce::ScopedJuceInitialiser_GUI init;
	Recorder recorder;
	Stage stage;
	RecorderControls controls (recorder);
	stage.attach (controls);

	controls.update ("stemdeck");
	EXPECT_EQ (controls.button.getButtonText(), "REC USB");
	EXPECT_TRUE (fits (controls.button));
	const auto usb = paintButton (controls);

	stage.stick = Stick::absent;
	controls.refreshTarget();
	controls.update ("stemdeck");
	EXPECT_EQ (controls.button.getButtonText(), "REC LOCAL");
	EXPECT_TRUE (fits (controls.button));
	const auto local = paintButton (controls);

	stage.stick = Stick::readOnly;
	controls.refreshTarget();
	controls.update ("stemdeck");
	EXPECT_EQ (controls.button.getButtonText(), "REC USB RO") << "a stick is there, and it is not a target";
	EXPECT_TRUE (fits (controls.button));
	const auto readOnly = paintButton (controls);

	const std::vector<const juce::Image*> faces { &usb, &local, &readOnly };
	for (const auto* image : faces)
		EXPECT_GT (distinctColours (*image), 2) << "the face painted more than a ground";
	for (size_t a = 0; a < faces.size(); ++a)
		for (size_t b = a + 1; b < faces.size(); ++b)
			EXPECT_GT (differingPixels (*faces[a], *faces[b]), 20) << a << " vs " << b;
}

TEST (RecorderControls, NoJackHasItsOwnFace)
{
	juce::ScopedJuceInitialiser_GUI init;
	Recorder recorder;
	Stage stage;
	RecorderControls controls (recorder);
	stage.attach (controls);

	controls.update ("stemdeck");
	const auto withJack = paintButton (controls);
	controls.update ({});
	EXPECT_TRUE (controls.button.getTooltip().containsIgnoreCase ("JACK"));
	// No JACK disables the button (the screens do it); disabled it must still paint, and differently.
	controls.button.setEnabled (false);
	const auto noJack = paintButton (controls);
	EXPECT_GT (distinctColours (noJack), 2);
	EXPECT_GT (differingPixels (withJack, noJack), 20);
}

TEST (RecorderControls, TheTargetShowsDuringATakeToo)
{
	juce::ScopedJuceInitialiser_GUI init;
	Recorder recorder;
	Stage stage;
	RecorderControls controls (recorder);
	stage.attach (controls);

	controls.toggle (44100.0);
	ASSERT_TRUE (recorder.isRecording());
	EXPECT_TRUE (controls.button.getButtonText().endsWith ("USB")) << controls.button.getButtonText();
	EXPECT_TRUE (fits (controls.button));
	const auto usbTake = paintButton (controls);
	controls.toggle (44100.0);

	stage.stick = Stick::absent;
	controls.toggle (44100.0);
	ASSERT_TRUE (recorder.isRecording());
	EXPECT_TRUE (controls.button.getButtonText().endsWith ("LOCAL")) << controls.button.getButtonText();
	EXPECT_TRUE (fits (controls.button));
	const auto localTake = paintButton (controls);
	controls.toggle (44100.0);

	EXPECT_GT (distinctColours (usbTake), 2);
	EXPECT_GT (distinctColours (localTake), 2);
	EXPECT_GT (differingPixels (usbTake, localTake), 20);
}

TEST (RecorderControls, AStickThatCannotBeWrittenTakesTheTakeLocal)
{
	juce::ScopedJuceInitialiser_GUI init;
	Recorder recorder;
	Stage stage;
	stage.writable = false;   // mounted rw on paper, read-only in fact
	RecorderControls controls (recorder);
	stage.attach (controls);

	controls.toggle (44100.0);
	ASSERT_TRUE (recorder.isRecording());
	EXPECT_TRUE (recorder.getFile().getFullPathName().contains ("stemdeck-RC-local"));
	controls.toggle (44100.0);
	EXPECT_EQ (controls.button.getButtonText(), "REC USB RO");
}

// The path an automatic stop takes is update() -> poll() -> face; no popup, a
// hand may be on the mixer. The face names the cause, then clears itself.
TEST (RecorderControls, APulledStickShowsOnTheButtonAndClearsItself)
{
	juce::ScopedJuceInitialiser_GUI init;
	Recorder recorder;
	Stage stage;
	RecorderControls controls (recorder);
	stage.attach (controls);

	controls.toggle (44100.0);
	ASSERT_TRUE (recorder.isRecording());
	const auto running = paintButton (controls);

	stage.stick = Stick::absent;
	stage.now += 1500;   // the mount table is read once a second
	controls.update ("stemdeck");

	EXPECT_FALSE (recorder.isRecording());
	EXPECT_EQ (controls.button.getButtonText(), "STICK GONE");
	EXPECT_TRUE (fits (controls.button));
	EXPECT_TRUE (controls.button.getTooltip().containsIgnoreCase ("removed"));
	const auto stopped = paintButton (controls);
	EXPECT_GT (distinctColours (stopped), 2);
	EXPECT_GT (differingPixels (running, stopped), 20);

	stage.now += RecorderControls::stoppedFaceMillis;
	controls.update ("stemdeck");
	EXPECT_EQ (controls.button.getButtonText(), "REC LOCAL") << "the face cleared by itself";
}

TEST (RecorderControls, TheMessageStaysUntilTheNextPressToo)
{
	juce::ScopedJuceInitialiser_GUI init;
	Recorder recorder;
	Stage stage;
	RecorderControls controls (recorder);
	stage.attach (controls);

	controls.toggle (44100.0);
	stage.stick = Stick::absent;
	stage.now += 1500;
	controls.update ("stemdeck");
	ASSERT_EQ (controls.button.getButtonText(), "STICK GONE");

	controls.toggle (44100.0);   // the press: a new take, locally now
	EXPECT_TRUE (recorder.isRecording());
	EXPECT_TRUE (controls.button.getButtonText().endsWith ("LOCAL"));
	controls.toggle (44100.0);
}

TEST (RecorderControls, AGapShowsInTheColourAndTheLongestFaceFits)
{
	juce::ScopedJuceInitialiser_GUI init;
	Recorder recorder;
	Stage stage;
	RecorderControls controls (recorder);
	stage.attach (controls);

	stage.stick = Stick::absent;
	controls.toggle (44100.0);
	ASSERT_TRUE (recorder.isRecording());
	const auto clean = paintButton (controls);

	recorder.markDroppedForTest();
	controls.update ("stemdeck");
	EXPECT_GT (differingPixels (clean, paintButton (controls)), 500) << "the whole face changes colour, nothing in the text to lose";
	EXPECT_TRUE (controls.button.getTooltip().contains ("gaps"));

	// The longest faces a take can wear: the longer target, the longest clock.
	EXPECT_EQ (RecorderControls::takeClock (125), "2:05");
	EXPECT_EQ (RecorderControls::takeClock (99 * 60 + 59), "99:59");
	EXPECT_EQ (RecorderControls::takeClock (2 * 3600 + 5 * 60), "2:05h");
	for (const auto* clock : { "99:59", "2:05h" })
	{
		controls.button.setButtonText (juce::String (clock) + " LOCAL");
		EXPECT_TRUE (fits (controls.button)) << clock;
	}
	for (const auto cause : { Recorder::StopCause::writeFailed, Recorder::StopCause::limitReached, Recorder::StopCause::targetGone })
	{
		controls.button.setButtonText (RecorderControls::stopFace (cause));
		EXPECT_TRUE (fits (controls.button)) << RecorderControls::stopFace (cause);
	}
	for (const auto* face : { "REC USB RO", "REC LOCAL" })
	{
		controls.button.setButtonText (face);
		EXPECT_TRUE (fits (controls.button)) << face;
	}
	controls.toggle (44100.0);
}

// The limit's face is spelled from the limit itself, not a second time.
TEST (RecorderControlsFace, TheLimitFaceNamesTheLimit)
{
	EXPECT_EQ (RecorderControls::stopFace (Recorder::StopCause::limitReached),
			   juce::String (RecordTarget::localLimitBytes / 1'000'000'000) + " GB FULL");
	EXPECT_EQ (RecorderControls::stopFace (Recorder::StopCause::none), juce::String());
}
