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

	// Smaller where a label would not fit its button: on the rig's 768 px
	// screen a deck's keys stand two to a row and MASTER came out "MAST...".
	juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
	// JUCE's own text indents leave a narrow key (a mixer's M, a deck's track
	// search) no room and draw "..."; ours are a share of the key.
	void drawButtonText (juce::Graphics&, juce::TextButton&, bool isHighlighted, bool isDown) override;

	// The library's header: the column it is sorted by lit, with a clear arrow.
	void drawTableHeaderColumn (juce::Graphics&, juce::TableHeaderComponent&, const juce::String& columnName, int columnId,
								int width, int height, bool isMouseOver, bool isMouseDown, int columnFlags) override;

	// A vertical fader's cap and slot, as shares of the fader: the cap's height
	// of its height, the slot's width of the cap's. JUCE keeps half a cap free
	// at each end of the travel (getSliderThumbRadius), so the cap reaches both.
	static float faderCapHeight (const juce::Component& fader) { return (float) fader.getHeight() * 0.11f; }
	static float faderCapWidth (const juce::Component& fader, juce::Rectangle<float> track)
	{
		return juce::jmin (faderCapHeight (fader) * 2.2f, track.getWidth() * 0.8f);
	}
	// A fader with this property set has meters behind its slot (the mixer's
	// volume faders: one per stem): the slot is as wide as the cap and left
	// open for them.
	static constexpr const char* meterInSlot = "meterInSlot";
	static juce::Rectangle<float> faderSlot (const juce::Component& fader, juce::Rectangle<float> track)
	{
		const auto share = fader.getProperties().getWithDefault (meterInSlot, false) ? 1.0f : 0.3f;
		return track.withSizeKeepingCentre (faderCapWidth (fader, track) * share, track.getHeight());
	}
	int getSliderThumbRadius (juce::Slider&) override;
};
