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

namespace
{
	int textIndent (const juce::Component& button)
	{
		return juce::jmax (1, juce::jmin (button.getWidth(), button.getHeight()) / 8);
	}
}

juce::Font DJLookAndFeel::getTextButtonFont (juce::TextButton& button, int buttonHeight)
{
	// A share of the key's height, and of its width for a tall narrow key
	// (JUCE's own caps it at 15 px); narrowed below where the label would
	// not fit.
	const juce::Font font (juce::FontOptions (juce::jmin ((float) buttonHeight * 0.45f, (float) button.getWidth() * 0.6f)));
	const auto room = (float) (button.getWidth() - 2 * textIndent (button));
	const auto wanted = juce::GlyphArrangement::getStringWidth (font, button.getButtonText());
	if (wanted <= room || wanted <= 0.0f)
		return font;
	return font.withHeight (font.getHeight() * room / wanted);
}

void DJLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
	g.setFont (getTextButtonFont (button, button.getHeight()));
	g.setColour (button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId)
					 .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));
	g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (textIndent (button)), juce::Justification::centred, 2);
}

void DJLookAndFeel::drawTableHeaderColumn (juce::Graphics& g, juce::TableHeaderComponent&, const juce::String& columnName, int,
										   int width, int height, bool, bool isMouseDown, int columnFlags)
{
	const auto forwards = (columnFlags & juce::TableHeaderComponent::sortedForwards) != 0;
	const auto sorted = forwards || (columnFlags & juce::TableHeaderComponent::sortedBackwards) != 0;
	auto area = juce::Rectangle<int> (width, height);

	if (sorted || isMouseDown)
	{
		g.setColour (Theme::loop.withAlpha (isMouseDown ? 0.45f : 0.25f));
		g.fillRect (area.reduced (1));
	}

	area.reduce (juce::jmax (2, height / 8), 0);

	if (sorted)
	{
		juce::Path arrow;
		arrow.addTriangle (0.0f, 0.0f, 0.5f, forwards ? -0.8f : 0.8f, 1.0f, 0.0f);
		g.setColour (Theme::loop);
		g.fillPath (arrow, arrow.getTransformToScaleToFit (area.removeFromRight (height / 2).reduced (height / 8).toFloat(), true));
	}

	g.setColour (sorted ? Theme::text : Theme::textDim);
	g.setFont (juce::FontOptions ((float) height * 0.4f, juce::Font::bold));
	g.drawFittedText (columnName, area, juce::Justification::centredLeft, 1);
}

juce::MouseCursor DJLookAndFeel::getMouseCursorFor (juce::Component& component)
{
	if (! pointerVisibility.isShown())
		return juce::MouseCursor::NoCursor;
	return LookAndFeel_V4::getMouseCursorFor (component);
}

void DJLookAndFeel::setPointerShown (bool shown)
{
	pointerVisibility.set (shown);
	// The cursor is only re-read when the mouse moves; ask for it now.
	for (auto source : juce::Desktop::getInstance().getMouseSources())
		source.forceMouseCursorUpdate();
}
