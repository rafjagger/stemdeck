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
	// Arc and body as shares of the knob, so a small one is a small knob and
	// not just a ring (the mixer's stem knobs, 2026-10-08).
	const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (1.0f);
	const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
	const auto centre = bounds.getCentre();
	const auto angle = startAngle + sliderPos * (endAngle - startAngle);
	const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
	const auto stroke = juce::jmax (1.5f, radius * 0.18f);
	const auto arcRadius = radius - stroke / 2.0f;

	// Track and value arc
	juce::Path track;
	track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
	g.setColour (Theme::outline);
	g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

	juce::Path value;
	value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
	g.setColour (slider.isEnabled() ? accent : accent.withAlpha (0.3f));
	g.strokePath (value, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

	// Knob body with pointer
	const auto bodyRadius = radius - stroke * 2.2f;
	g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3d42), centre.x, centre.y - bodyRadius,
											 juce::Colour (0xff222428), centre.x, centre.y + bodyRadius, false));
	g.fillEllipse (juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre));

	juce::Path pointer;
	const auto pointerWidth = juce::jmax (1.5f, bodyRadius * 0.2f);
	pointer.addRoundedRectangle (-pointerWidth / 2.0f, -bodyRadius * 0.9f, pointerWidth, bodyRadius * 0.5f, pointerWidth / 2.0f);
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

	// Mixer-style fader: a slot with tick marks and a wide cap, all shares of
	// the fader; the slot left open where a meter stands behind it.
	const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
	const auto slot = faderSlot (slider, bounds);
	const auto capWidth = faderCapWidth (slider, bounds);
	const auto tickGap = slot.getWidth() * 0.5f;

	g.setColour (Theme::outline);
	for (int i = 0; i <= 10; ++i)
	{
		const auto tickY = bounds.getY() + bounds.getHeight() * (float) i / 10.0f;
		const auto tickW = capWidth * ((i % 5 == 0) ? 0.22f : 0.13f);
		g.drawHorizontalLine (juce::roundToInt (tickY), slot.getX() - tickW - tickGap, slot.getX() - tickGap);
		g.drawHorizontalLine (juce::roundToInt (tickY), slot.getRight() + tickGap, slot.getRight() + tickW + tickGap);
	}

	if (! slider.getProperties().getWithDefault (meterInSlot, false))
	{
		g.setColour (juce::Colours::black);
		g.fillRoundedRectangle (slot.withSizeKeepingCentre (juce::jmax (2.0f, slot.getWidth() * 0.4f), slot.getHeight()), 2.0f);
	}

	const auto cap = juce::Rectangle<float> (capWidth, faderCapHeight (slider)).withCentre ({ bounds.getCentreX(), sliderPos });

	g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4d53), 0.0f, cap.getY(),
											 juce::Colour (0xff2b2d31), 0.0f, cap.getBottom(), false));
	g.fillRoundedRectangle (cap, 3.0f);
	g.setColour (slider.findColour (juce::Slider::thumbColourId));
	g.fillRect (cap.withSizeKeepingCentre (capWidth * 0.85f, juce::jmax (2.0f, cap.getHeight() * 0.1f)));
}

int DJLookAndFeel::getSliderThumbRadius (juce::Slider& slider)
{
	if (slider.getSliderStyle() != juce::Slider::LinearVertical)
		return LookAndFeel_V4::getSliderThumbRadius (slider);
	return juce::roundToInt (faderCapHeight (slider) / 2.0f);
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

juce::Font DJLookAndFeel::getTextButtonFont (juce::TextButton& button, int buttonHeight)
{
	const auto font = juce::LookAndFeel_V4::getTextButtonFont (button, buttonHeight);
	const auto room = (float) button.getWidth() - 8.0f;
	const auto wanted = juce::GlyphArrangement::getStringWidth (font, button.getButtonText());
	if (wanted <= room || wanted <= 0.0f)
		return font;
	return font.withHeight (juce::jmax (9.0f, font.getHeight() * room / wanted));
}
