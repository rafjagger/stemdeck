#pragma once

#include <JuceHeader.h>
#include "StemDeckPlayer.h"
#include "Buses.h"
#include "Session.h"

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
// Meters for all output channels: buses 1-4, AUX and PHONES, each L/R.
class OutputMeters : public juce::Component
{
public:
	static constexpr int numBuses = buses::count;
	static constexpr int numChannels = numBuses * 2;

	OutputMeters();

	void setLevel (int channel, float peak) { meters[channel]->setLevel (peak); }
	static juce::String busName (int bus) { return juce::String (buses::name (bus)); }

	void paint (juce::Graphics& g) override;
	void resized() override;

private:
	juce::OwnedArray<LevelMeter> meters;
	juce::Rectangle<int> labelArea;
};

//==============================================================================
// Channel strip of one deck: a knob per stem, with mute and one switch per
// bus (1-4, AUX, PH -- any number at once, Buses.h), above the channel fader
// and the deck's PHONES button (the whole deck to PHONES, pre fader).
class ChannelStrip : public juce::Component
{
public:
	ChannelStrip (StemDeckPlayer& player, int deckIndex);

	void setStemNames (const std::array<juce::String, StemSet::numStems>& names);
	void toggleMute (int stem);
	void refresh(); // meters, called by the main timer

	// Knobs, mutes, bus switches, fader and PHONES, for the session.
	void saveState (DeckSession& state) const;
	void restoreState (const DeckSession& state);

	// Room left free beside the fader on the side towards the mixer's middle,
	// for the output meters; where that room is, in this strip's coordinates.
	void setMeterReserve (int width) { meterReserve = width; resized(); }
	juce::Rectangle<int> getMeterZone() const { return meterZone; }

	void paint (juce::Graphics& g) override;
	void resized() override;

private:
	StemDeckPlayer& player;
	const int deckIndex;

	juce::OwnedArray<juce::Slider> knobs;
	juce::OwnedArray<juce::TextButton> muteButtons, busButtons;   // busButtons: stem * buses::count + bus
	juce::TextButton phonesButton { "PHONES" };
	juce::OwnedArray<juce::Label> stemLabels;
	juce::Slider fader { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
	LevelMeter meter;

	int meterReserve = 0;
	juce::Rectangle<int> meterZone;
	std::array<juce::Rectangle<int>, StemSet::numStems> stemFrames;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStrip)
};

//==============================================================================
class MixerPanel : public juce::Component
{
public:
	MixerPanel (StemDeckPlayer& playerA, StemDeckPlayer& playerB);

	ChannelStrip& strip (int deckIndex) { return deckIndex == 0 ? stripA : stripB; }
	const ChannelStrip& strip (int deckIndex) const { return deckIndex == 0 ? stripA : stripB; }
	void refresh();
	void setOutputLevel (int channel, float peak) { outputMeters.setLevel (channel, peak); }

	void paint (juce::Graphics& g) override;
	void resized() override;

private:
	ChannelStrip stripA, stripB;
	OutputMeters outputMeters;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerPanel)
};
