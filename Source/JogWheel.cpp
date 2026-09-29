#include "JogWheel.h"
#include "Theme.h"

namespace
{
	constexpr float platterFraction = 0.78f;  // inside this radius is the top
	constexpr double bendPerTurnPerSecond = 0.15; // one turn per second on the ring = +15 %
	constexpr double maxBend = 0.5;
	constexpr double searchSecondsPerTurn = 4.0;  // ring while stopped

	float wrapAngle (float a)
	{
		while (a > juce::MathConstants<float>::pi)  a -= juce::MathConstants<float>::twoPi;
		while (a < -juce::MathConstants<float>::pi) a += juce::MathConstants<float>::twoPi;
		return a;
	}
}

JogWheel::JogWheel (StemDeckPlayer& p, int index) : player (p), deckIndex (index)
{
	setRepaintsOnMouseActivity (false);
}

float JogWheel::angleAt (juce::Point<float> p) const
{
	const auto c = centre();
	return std::atan2 (p.y - c.y, p.x - c.x);
}

void JogWheel::mouseDown (const juce::MouseEvent& e)
{
	if (! player.isLoaded())
		return;

	const auto distance = e.position.getDistanceFrom (centre()) / radius();
	touch = (distance <= platterFraction && vinylMode) ? Touch::platter : Touch::ring;
	lastAngle = angleAt (e.position);
	lastMoveTime = juce::Time::getMillisecondCounterHiRes();

	if (touch == Touch::platter)
		player.beginScratch();

	repaint();
}

void JogWheel::mouseDrag (const juce::MouseEvent& e)
{
	if (touch == Touch::none)
		return;

	const auto angle = angleAt (e.position);
	const auto delta = wrapAngle (angle - lastAngle);
	lastAngle = angle;

	const auto now = juce::Time::getMillisecondCounterHiRes();
	const auto elapsed = juce::jmax (1.0, now - lastMoveTime) / 1000.0;
	lastMoveTime = now;

	const auto turns = (double) delta / juce::MathConstants<double>::twoPi;

	if (touch == Touch::platter)
	{
		player.scratchBy (turns * secondsPerRevolution);
	}
	else if (player.isPlaying())
	{
		bend = juce::jlimit (1.0 - maxBend, 1.0 + maxBend, 1.0 + (turns / elapsed) * bendPerTurnPerSecond);
		player.setPitchBend (bend);
	}
	else
	{
		player.setPosition (player.getPosition() + turns * searchSecondsPerTurn);
	}
}

void JogWheel::mouseUp (const juce::MouseEvent&)
{
	if (touch == Touch::platter)
		player.endScratch();

	touch = Touch::none;
	repaint();
}

void JogWheel::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
	if (! player.isLoaded())
		return;

	if (player.isPlaying())
	{
		bend = juce::jlimit (1.0 - maxBend, 1.0 + maxBend, bend + wheel.deltaY * 0.1);
		player.setPitchBend (bend);
		lastMoveTime = juce::Time::getMillisecondCounterHiRes();
	}
	else
	{
		player.setPosition (player.getPosition() + wheel.deltaY * 0.25);
	}
}

void JogWheel::refresh()
{
	// The bend lasts while the ring keeps turning and eases off when it stops.
	const auto idle = juce::Time::getMillisecondCounterHiRes() - lastMoveTime > 60.0;

	if (std::abs (bend - 1.0) > 1e-4 && (idle || touch != Touch::ring))
	{
		bend = 1.0 + (bend - 1.0) * 0.8;

		if (std::abs (bend - 1.0) <= 1e-4)
			bend = 1.0;

		player.setPitchBend (bend);
	}

	repaint();
}

void JogWheel::paint (juce::Graphics& g)
{
	const auto c = centre();
	const auto r = radius();
	const auto deckColour = Theme::deck (deckIndex);
	const auto circle = [c] (float rad) { return juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c); };

	// Ring with grip ticks
	g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3d42), c.x, c.y - r, juce::Colour (0xff17181b), c.x, c.y + r, false));
	g.fillEllipse (circle (r));
	g.setColour (juce::Colour (0xff4a4d53));

	for (int i = 0; i < 72; ++i)
	{
		const auto a = (float) i / 72.0f * juce::MathConstants<float>::twoPi;
		const auto inner = r * (platterFraction + 0.05f), outer = r * 0.97f;
		g.drawLine (c.x + std::cos (a) * inner, c.y + std::sin (a) * inner, c.x + std::cos (a) * outer, c.y + std::sin (a) * outer, 1.0f);
	}

	// Platter
	const auto platter = r * platterFraction;
	g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2e33), c.x, c.y - platter, juce::Colour (0xff0e0f11), c.x, c.y + platter, false));
	g.fillEllipse (circle (platter));

	if (touch == Touch::platter)
	{
		g.setColour (deckColour.withAlpha (0.8f));
		g.drawEllipse (circle (platter - 1.5f), 3.0f);
	}
	else
	{
		g.setColour (Theme::outline);
		g.drawEllipse (circle (platter), 1.0f);
	}

	if (! player.isLoaded())
		return;

	// Marker turning with the playhead
	const auto position = player.getPosition();
	const auto angle = (float) (std::fmod (position / secondsPerRevolution, 1.0) * juce::MathConstants<double>::twoPi)
					 - juce::MathConstants<float>::halfPi;
	const auto markerInner = platter * 0.55f, markerOuter = platter * 0.95f;
	g.setColour (Theme::text);
	g.drawLine (c.x + std::cos (angle) * markerInner, c.y + std::sin (angle) * markerInner,
				c.x + std::cos (angle) * markerOuter, c.y + std::sin (angle) * markerOuter, 4.0f);

	// Centre display: track progress arc and effective BPM
	const auto display = platter * 0.5f;
	g.setColour (juce::Colours::black);
	g.fillEllipse (circle (display));

	const auto length = player.getLength();
	if (length > 0.0)
	{
		juce::Path progress;
		progress.addCentredArc (c.x, c.y, display - 4.0f, display - 4.0f, 0.0f, 0.0f,
								(float) (position / length) * juce::MathConstants<float>::twoPi, true);
		g.setColour (deckColour);
		g.strokePath (progress, juce::PathStrokeType (3.0f));
	}

	const auto grid = player.getBeatGrid();
	g.setColour (Theme::text);
	g.setFont (juce::FontOptions (display * 0.42f, juce::Font::bold));
	g.drawText (grid.isValid() ? juce::String (grid.bpm * player.getEffectiveRate(), 1) : juce::String ("--"),
				circle (display).translated (0.0f, -display * 0.12f), juce::Justification::centred);
	g.setColour (Theme::textDim);
	g.setFont (juce::FontOptions (display * 0.2f));
	g.drawText ("BPM", circle (display).translated (0.0f, display * 0.38f), juce::Justification::centred);
}
