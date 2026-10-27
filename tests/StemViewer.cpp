// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "StemViewer.h"

#include <cmath>
#include <optional>

// The viewer is a component: it needs JUCE's GUI initialised, no X window.
namespace
{
	juce::MouseEvent mouseAt (StemViewer& viewer, juce::Point<float> where, juce::Point<float> downAt)
	{
		const auto now = juce::Time::getCurrentTime();
		return { juce::Desktop::getInstance().getMainMouseSource(), where, juce::ModifierKeys::leftButtonModifier,
				 juce::MouseInputSource::defaultPressure, 0.0f, 0.0f, 0.0f, 0.0f, &viewer, &viewer, now, downAt, now, 1,
				 where != downAt };
	}

	void drag (StemViewer& viewer, juce::Point<float> from, juce::Point<float> to)
	{
		viewer.mouseDown (mouseAt (viewer, from, from));
		viewer.mouseDrag (mouseAt (viewer, (from + to) / 2.0f, from));
		viewer.mouseDrag (mouseAt (viewer, to, from));
		viewer.mouseUp (mouseAt (viewer, to, from));
	}

	float azimuthDifference (float a, float b) { return std::remainder (a - b, 360.0f); }
}

TEST (StemViewer, DragMovesHome)
{
	juce::ScopedJuceInitialiser_GUI init;
	StemViewer viewer;
	viewer.setSize (300, 300);
	const spatial::Pose home { 0.0f, 0.0f, 60.0f };
	viewer.setShown (home, home, motionlite::PadState::idle);

	std::optional<spatial::Pose> moved;
	viewer.onHomeChanged = [&] (spatial::Pose pose) { moved = pose; };

	// The centre dot to azimuth +90 (left), elevation 45: direction moves, width stays.
	drag (viewer, viewer.pointOf (0.0f, 0.0f), viewer.pointOf (90.0f, 45.0f));
	ASSERT_TRUE (moved.has_value());
	EXPECT_NEAR (azimuthDifference (moved->azimuth, 90.0f), 0.0f, 1.0f);
	EXPECT_NEAR (moved->elevation, 45.0f, 1.0f);
	EXPECT_FLOAT_EQ (moved->width, 60.0f);

	// The L dot outwards: the width grows, the direction stays.
	viewer.setShown (*moved, *moved, motionlite::PadState::idle);
	const auto dots = viewer.dotsFor (*moved);
	const auto pulled = *moved;
	moved.reset();
	drag (viewer, dots.left, viewer.pointOf (pulled.azimuth + 70.0f, 0.0f));
	ASSERT_TRUE (moved.has_value());
	EXPECT_GT (moved->width, 100.0f);
	EXPECT_FLOAT_EQ (moved->azimuth, pulled.azimuth);
	EXPECT_FLOAT_EQ (moved->elevation, pulled.elevation);

	// While a shot runs the viewer only shows.
	viewer.setShown (home, { 45.0f, 10.0f, 60.0f }, motionlite::PadState::running);
	moved.reset();
	drag (viewer, viewer.pointOf (45.0f, 10.0f), viewer.pointOf (-90.0f, 30.0f));
	EXPECT_FALSE (moved.has_value());
}

// The dots sit where IEM's StereoEncoder puts L and R: at azimuth +-width/2
// around the direction on the horizon, front at the top, left to the left.
TEST (StemViewer, DotsFollowIEMsStereoPair)
{
	juce::ScopedJuceInitialiser_GUI init;
	StemViewer viewer;
	viewer.setSize (300, 300);
	const auto dots = viewer.dotsFor ({ 0.0f, 0.0f, 90.0f });
	const auto centre = viewer.getLocalBounds().toFloat().getCentre();
	EXPECT_LT (dots.centre.y, centre.y) << "front is up";
	EXPECT_LT (dots.left.x, centre.x) << "left is left";
	EXPECT_GT (dots.right.x, centre.x);
	EXPECT_NEAR (dots.left.y, dots.right.y, 0.5f);

	const auto top = viewer.dotsFor ({ 0.0f, 90.0f, 0.0f });
	EXPECT_NEAR (top.centre.getDistanceFrom (centre), 0.0f, 0.5f) << "straight up is the middle";
}

TEST (StemViewer, PaintsIdleArmedRunning)
{
	juce::ScopedJuceInitialiser_GUI init;
	StemViewer viewer;
	viewer.setColour (StemViewer::stemColourId, juce::Colours::orange);
	viewer.setSize (240, 240);
	const spatial::Pose home { 30.0f, 10.0f, 60.0f }, now { -60.0f, 40.0f, 60.0f };

	std::array<juce::Image, 3> images;
	const motionlite::PadState states[] = { motionlite::PadState::idle, motionlite::PadState::armed,
											motionlite::PadState::running };
	for (size_t i = 0; i < images.size(); ++i)
	{
		viewer.setShown (home, now, states[i]);
		images[i] = viewer.createComponentSnapshot (viewer.getLocalBounds());
		ASSERT_EQ (images[i].getWidth(), 240);
	}

	// Running shows the running pose: its centre dot is lit where home is not.
	const auto atNow = viewer.dotsFor (now).centre.toInt();
	const auto lit = [] (const juce::Image& image, juce::Point<int> p) { return image.getPixelAt (p.x, p.y).getBrightness(); };
	EXPECT_GT (lit (images[2], atNow), lit (images[0], atNow) + 0.2f);
	EXPECT_NE (images[0].getPixelAt (viewer.dotsFor (home).centre.toInt().x, viewer.dotsFor (home).centre.toInt().y),
			   images[1].getPixelAt (viewer.dotsFor (home).centre.toInt().x, viewer.dotsFor (home).centre.toInt().y))
		<< "armed looks different from idle at home";
}
