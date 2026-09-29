#include "Theme.h"

DJLookAndFeel::DJLookAndFeel()
{
	setColourScheme ({ Theme::panel, Theme::background, Theme::panelRaised, Theme::outline, Theme::text,
					   Theme::panelRaised, Theme::text, Theme::panelRaised, Theme::text });

	setColour (juce::ResizableWindow::backgroundColourId, Theme::background);
	setColour (juce::TextButton::buttonColourId, Theme::panelRaised);
	setColour (juce::TextButton::textColourOffId, Theme::text);
	setColour (juce::TextButton::textColourOnId, juce::Colours::black);
	setColour (juce::Label::textColourId, Theme::text);
	setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
	setColour (juce::TooltipWindow::backgroundColourId, Theme::panelRaised);
	setColour (juce::TooltipWindow::textColourId, Theme::text);
}

void DJLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
									  float startAngle, float endAngle, juce::Slider& slider)
{
	const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
	const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
	const auto centre = bounds.getCentre();
	const auto angle = startAngle + sliderPos * (endAngle - startAngle);
	const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
	const auto arcRadius = radius - 2.0f;

	// Track and value arc
	juce::Path track;
	track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
	g.setColour (Theme::outline);
	g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

	juce::Path value;
	value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
	g.setColour (slider.isEnabled() ? accent : accent.withAlpha (0.3f));
	g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

	// Knob body with pointer
	const auto bodyRadius = radius - 6.0f;
	g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3d42), centre.x, centre.y - bodyRadius,
											 juce::Colour (0xff222428), centre.x, centre.y + bodyRadius, false));
	g.fillEllipse (juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre));

	juce::Path pointer;
	pointer.addRoundedRectangle (-1.5f, -bodyRadius + 2.0f, 3.0f, bodyRadius * 0.5f, 1.5f);
	g.setColour (Theme::text);
	g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void DJLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
									  float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle style, juce::Slider& slider)
{
	if (style != juce::Slider::LinearVertical)
	{
		LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
		return;
	}

	// Mixer-style fader: thin slot with tick marks and a wide cap.
	const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
	const auto slot = juce::Rectangle<float> (4.0f, bounds.getHeight()).withCentre (bounds.getCentre());

	g.setColour (Theme::outline);
	for (int i = 0; i <= 10; ++i)
	{
		const auto tickY = bounds.getY() + bounds.getHeight() * (float) i / 10.0f;
		const auto tickW = (i % 5 == 0) ? 10.0f : 6.0f;
		g.drawHorizontalLine (juce::roundToInt (tickY), slot.getX() - tickW - 4.0f, slot.getX() - 4.0f);
		g.drawHorizontalLine (juce::roundToInt (tickY), slot.getRight() + 4.0f, slot.getRight() + tickW + 4.0f);
	}

	g.setColour (juce::Colours::black);
	g.fillRoundedRectangle (slot, 2.0f);

	const auto capWidth = juce::jmin (bounds.getWidth() - 4.0f, 36.0f);
	const auto cap = juce::Rectangle<float> (capWidth, 22.0f).withCentre ({ bounds.getCentreX(), sliderPos });

	g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4d53), 0.0f, cap.getY(),
											 juce::Colour (0xff2b2d31), 0.0f, cap.getBottom(), false));
	g.fillRoundedRectangle (cap, 3.0f);
	g.setColour (slider.findColour (juce::Slider::thumbColourId));
	g.fillRect (cap.withSizeKeepingCentre (capWidth - 6.0f, 2.0f));
}

void DJLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
										  bool isMouseOverButton, bool isButtonDown)
{
	auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
	auto colour = backgroundColour;

	if (isButtonDown)
		colour = colour.brighter (0.25f);
	else if (isMouseOverButton)
		colour = colour.brighter (0.08f);

	g.setColour (colour);
	g.fillRoundedRectangle (bounds, 4.0f);
	g.setColour (Theme::outline);
	g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
}
