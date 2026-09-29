#pragma once

#include <JuceHeader.h>
#include "StemDeckPlayer.h"

// Vertical peak meter fed from a player's stem peaks.
class LevelMeter : public juce::Component
{
public:
	void setLevel (float newPeak);
	void paint (juce::Graphics& g) override;

private:
	float level = 0.0f;
};

//==============================================================================
// Meters for all output channels: buses 1-4 and aux, each L/R.
class OutputMeters : public juce::Component
{
public:
	static constexpr int numBuses = 5;
	static constexpr int numChannels = numBuses * 2;

	OutputMeters();

	void setLevel (int channel, float peak) { meters[channel]->setLevel (peak); }
	static juce::String busName (int bus) { return bus < numBuses - 1 ? juce::String (bus + 1) : juce::String ("AUX"); }

	void paint (juce::Graphics& g) override;
	void resized() override;

private:
	juce::OwnedArray<LevelMeter> meters;
	juce::Rectangle<int> labelArea;
};

//==============================================================================
// Channel strip of one deck: a knob per stem (with mute and aux send) above
// the channel fader. AUX takes the stem off its main bus and sends it,
// post fader, to the aux bus instead.
class ChannelStrip : public juce::Component
{
public:
	ChannelStrip (StemDeckPlayer& player, int deckIndex);

	void setStemNames (const std::array<juce::String, StemSet::numStems>& names);
	void toggleMute (int stem);
	void refresh(); // meters, called by the main timer

	void paint (juce::Graphics& g) override;
	void resized() override;

private:
	StemDeckPlayer& player;
	const int deckIndex;

	juce::OwnedArray<juce::Slider> knobs;
	juce::OwnedArray<juce::TextButton> muteButtons, auxButtons;
	juce::OwnedArray<juce::Label> stemLabels;
	juce::Slider fader { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
	LevelMeter meter;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStrip)
};

//==============================================================================
class MixerPanel : public juce::Component
{
public:
	MixerPanel (StemDeckPlayer& playerA, StemDeckPlayer& playerB);

	ChannelStrip& strip (int deckIndex) { return deckIndex == 0 ? stripA : stripB; }
	void refresh();
	void setOutputLevel (int channel, float peak) { outputMeters.setLevel (channel, peak); }

	void paint (juce::Graphics& g) override;
	void resized() override;

private:
	ChannelStrip stripA, stripB;
	OutputMeters outputMeters;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerPanel)
};
