// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "StemViewer.h"

#include <cmath>

namespace
{
// Shares of the disc's radius.
constexpr float discShare = 0.92f, dotShare = 0.08f, grabShare = 0.2f;
constexpr float ghostAlpha = 0.35f, gridAlpha = 0.18f, lineAlpha = 0.5f;

struct Direction { float azimuth, elevation; };
struct Vector { double x, y, z; };   // x front, y left, z up

double radians (double degrees) { return degrees * juce::MathConstants<double>::pi / 180.0; }
double degrees (double radians) { return radians * 180.0 / juce::MathConstants<double>::pi; }

Vector rotate (const Vector& v, double azimuth, double elevation)
{
	const double ce = std::cos (elevation), se = std::sin (elevation), ca = std::cos (azimuth), sa = std::sin (azimuth);
	const Vector pitched { v.x * ce - v.z * se, v.y, v.x * se + v.z * ce };
	return { pitched.x * ca - pitched.y * sa, pitched.x * sa + pitched.y * ca, pitched.z };
}

Vector unrotate (const Vector& v, double azimuth, double elevation)
{
	const double ce = std::cos (elevation), se = std::sin (elevation), ca = std::cos (azimuth), sa = std::sin (azimuth);
	const Vector yawed { v.x * ca + v.y * sa, -v.x * sa + v.y * ca, v.z };
	return { yawed.x * ce + yawed.z * se, yawed.y, -yawed.x * se + yawed.z * ce };
}

Vector vectorOf (Direction d)
{
	return rotate ({ 1.0, 0.0, 0.0 }, radians (d.azimuth), radians (d.elevation));
}

Direction directionOf (const Vector& v)
{
	return { (float) degrees (std::atan2 (v.y, v.x)), (float) degrees (std::asin (juce::jlimit (-1.0, 1.0, v.z))) };
}

// IEM StereoEncoder: L and R at +-width/2 around the direction in the pair's
// own plane, turned with it (quatL = direction * rotation about z by width/2).
Direction channelOf (const spatial::Pose& pose, double side)
{
	const double half = radians (pose.width) / 2.0 * side;
	return directionOf (rotate ({ std::cos (half), std::sin (half), 0.0 }, radians (pose.azimuth), radians (pose.elevation)));
}
}

StemViewer::StemViewer()
{
	setOpaque (true);
}

namespace
{
bool samePose (const spatial::Pose& a, const spatial::Pose& b)
{
	return juce::exactlyEqual (a.azimuth, b.azimuth) && juce::exactlyEqual (a.elevation, b.elevation) && juce::exactlyEqual (a.width, b.width);
}
}

// Called by a timer: repaints only when something it shows has changed.
void StemViewer::setShown (spatial::Pose newHome, spatial::Pose newNow, motionlite::PadState newState)
{
	if (newState == motionlite::PadState::running)
		grab = Grab::none;
	if (samePose (home, newHome) && samePose (now, newNow) && state == newState)
		return;
	home = newHome;
	now = newNow;
	state = newState;
	repaint();
}

juce::Point<float> StemViewer::middle() const
{
	return getLocalBounds().toFloat().getCentre();
}

float StemViewer::discRadius() const
{
	return (float) juce::jmin (getWidth(), getHeight()) / 2.0f * discShare;
}

float StemViewer::dotRadius() const
{
	return discRadius() * dotShare;
}

// Elevation as distance from the middle, linear: the rim is 0, the middle 90.
juce::Point<float> StemViewer::pointOf (float azimuth, float elevation) const
{
	const auto r = discRadius() * (90.0f - std::abs (juce::jlimit (-90.0f, 90.0f, elevation))) / 90.0f;
	const auto a = (float) radians (azimuth);
	return middle() + juce::Point<float> (-r * std::sin (a), -r * std::cos (a));
}

StemViewer::Dots StemViewer::dotsFor (const spatial::Pose& pose) const
{
	const auto left = channelOf (pose, 1.0), right = channelOf (pose, -1.0);
	return { pointOf (pose.azimuth, pose.elevation), pointOf (left.azimuth, left.elevation),
			 pointOf (right.azimuth, right.elevation) };
}

void StemViewer::paint (juce::Graphics& g)
{
	g.fillAll (findColour (juce::ResizableWindow::backgroundColourId));
	paintGrid (g);
	const auto stem = findColour (stemColourId);
	switch (state)
	{
		case motionlite::PadState::idle: paintPair (g, home, stem, true); return;
		case motionlite::PadState::armed: paintPair (g, home, stem, false); return;   // waiting for the bar
		case motionlite::PadState::running:
			paintPair (g, home, stem.withAlpha (ghostAlpha), false);
			paintPair (g, now, stem, true);
			return;
	}
}

void StemViewer::paintGrid (juce::Graphics& g) const
{
	const auto grid = findColour (juce::Label::textColourId).withAlpha (gridAlpha);
	const auto centre = middle();
	const auto radius = discRadius();
	const auto stroke = juce::jmax (1.0f, radius * 0.01f);
	g.setColour (grid);
	for (const float elevation : { 0.0f, 30.0f, 60.0f })
	{
		const auto r = radius * (90.0f - elevation) / 90.0f;
		g.drawEllipse (juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (centre), stroke);
	}
	g.drawLine ({ centre.translated (0.0f, -radius), centre.translated (0.0f, radius) }, stroke);
	g.drawLine ({ centre.translated (-radius, 0.0f), centre.translated (radius, 0.0f) }, stroke);
	g.fillEllipse (juce::Rectangle<float> (dotRadius(), dotRadius()).withCentre (centre.translated (0.0f, -radius)));   // front
}

void StemViewer::paintPair (juce::Graphics& g, const spatial::Pose& pose, juce::Colour colour, bool filled) const
{
	const auto dots = dotsFor (pose);
	const auto dot = dotRadius();
	const auto stroke = juce::jmax (1.0f, dot * 0.25f);
	g.setColour (colour.withMultipliedAlpha (lineAlpha));
	g.drawLine ({ dots.left, dots.centre }, stroke);
	g.drawLine ({ dots.centre, dots.right }, stroke);

	const auto drawDot = [&] (juce::Point<float> at, float radius, const juce::String& name)
	{
		const auto area = juce::Rectangle<float> (2.0f * radius, 2.0f * radius).withCentre (at);
		g.setColour (colour);
		if (filled)
			g.fillEllipse (area);
		else
			g.drawEllipse (area.reduced (stroke / 2.0f), stroke);
		if (name.isEmpty())
			return;
		g.setColour (filled ? findColour (juce::ResizableWindow::backgroundColourId) : colour);
		g.setFont (juce::FontOptions (radius * 1.4f, juce::Font::bold));
		g.drawText (name, area, juce::Justification::centred, false);
	};
	drawDot (dots.left, dot * 1.3f, "L");
	drawDot (dots.right, dot * 1.3f, "R");
	drawDot (dots.centre, dot, {});
}

void StemViewer::mouseDown (const juce::MouseEvent& e)
{
	grab = Grab::none;
	if (state == motionlite::PadState::running)
		return;
	const auto dots = dotsFor (home);
	auto nearest = discRadius() * grabShare;
	for (const auto& [which, at] : { std::pair { Grab::centre, dots.centre }, std::pair { Grab::left, dots.left },
									 std::pair { Grab::right, dots.right } })
	{
		const auto distance = e.position.getDistanceFrom (at);
		if (distance < nearest)
		{
			nearest = distance;
			grab = which;
		}
	}
}

void StemViewer::mouseDrag (const juce::MouseEvent& e)
{
	if (grab == Grab::none || state == motionlite::PadState::running)
		return;
	home = dragged (e.position);
	repaint();
	if (onHomeChanged)
		onHomeChanged (home);
}

void StemViewer::mouseUp (const juce::MouseEvent&)
{
	grab = Grab::none;
}

// The middle dot follows the pointer (in home's hemisphere); L or R turn the
// width to the pointer's angle in the pair's own plane.
spatial::Pose StemViewer::dragged (juce::Point<float> at) const
{
	const auto offset = at - middle();
	const auto r = juce::jlimit (0.0f, 1.0f, offset.getDistanceFromOrigin() / discRadius());
	const auto azimuth = (float) degrees (std::atan2 (-offset.x, -offset.y));
	const auto elevation = std::copysign (90.0f * (1.0f - r), home.elevation < 0.0f ? -1.0f : 1.0f);

	auto pose = home;
	if (grab == Grab::centre)
	{
		pose.azimuth = azimuth;
		pose.elevation = elevation;
		return pose;
	}
	const auto local = unrotate (vectorOf ({ azimuth, elevation }), radians (home.azimuth), radians (home.elevation));
	const auto side = grab == Grab::left ? 1.0 : -1.0;
	auto half = degrees (std::atan2 (side * local.y, local.x));
	if (half < 0.0)
		half = 0.0;   // across the middle: the pair closes, it does not swap
	pose.width = (float) juce::jlimit (0.0, 360.0, 2.0 * half);
	return pose;
}
