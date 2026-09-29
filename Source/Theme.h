#pragma once

#include <JuceHeader.h>

// Colours and look of the app, loosely after Mixxx's dark skins.
namespace Theme
{
	inline const juce::Colour background   { 0xff121315 };
	inline const juce::Colour panel        { 0xff1c1d20 };
	inline const juce::Colour panelRaised  { 0xff26282c };
	inline const juce::Colour outline      { 0xff34363b };
	inline const juce::Colour text         { 0xffdcdcdc };
	inline const juce::Colour textDim      { 0xff8b8e94 };
	inline const juce::Colour play         { 0xff3ec46d };
	inline const juce::Colour cue          { 0xffe8a33d };
	inline const juce::Colour mute         { 0xffd04545 };
	inline const juce::Colour loop         { 0xff5fb0ff };
	inline const juce::Colour aux          { 0xff9b6bff };

	inline juce::Colour stem (int index)
	{
		static const juce::Colour colours[] = { juce::Colour (0xffe8a33d), juce::Colour (0xff4fb3bf),
												juce::Colour (0xffd06a86), juce::Colour (0xff8f9bd6) };
		return colours[index & 3];
	}

	inline juce::Colour deck (int index)
	{
		return index == 0 ? juce::Colour (0xff4a9eff) : juce::Colour (0xffff8a3d);
	}

	inline juce::String deckName (int index)
	{
		return index == 0 ? "A" : "B";
	}

	inline juce::String formatTime (double seconds)
	{
		const auto total = juce::jmax (0.0, seconds);
		const auto minutes = (int) (total / 60.0);
		return juce::String (minutes) + ":" + juce::String (total - minutes * 60.0, 1).paddedLeft ('0', 4);
	}
}

//==============================================================================
class DJLookAndFeel : public juce::LookAndFeel_V4
{
public:
	DJLookAndFeel();

	void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
						   float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

	void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
						   float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;

	void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
							   bool isMouseOverButton, bool isButtonDown) override;
};
