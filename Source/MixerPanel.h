#pragma once

#include <JuceHeader.h>
#include "StemDeckPlayer.h"
#include "Buses.h"
#include "Session.h"
#include "MeterBallistics.h"

// Vertical peak meter fed from a player's stem peaks, with the system's
// ballistics (PeakMeter) and a line where the held peak stands.
// With a clip lamp, its top segment lights only above full scale and holds.
class LevelMeter : public juce::Component
{
public:
	void setLevel (float newPeak);
	void setParameters (MeterParameters parameters) { ballistics.setParameters (parameters); }
	void setShowsClip (bool shows) { showsClip = shows; repaint(); }
	void paint (juce::Graphics& g) override;

private:
	float secondsSinceLastFeed();

	PeakMeter ballistics;
	double lastFeedMs = 0.0;
	int litSegments = 0, heldSegment = 0;
	bool showsClip = false;
	ClipHold clipHold;
	bool clipping = false;
};

//==============================================================================
// Meters for all output channels: buses 1-4 and AUX, each L/R.
class OutputMeters : public juce::Component
{
public:
	static constexpr int numBuses = buses::count;
	static constexpr int numChannels = numBuses * 2;

	OutputMeters();

	void setLevel (int channel, float peak) { meters[channel]->setLevel (peak); }
	void setMeterParameters (MeterParameters parameters)
	{
		for (auto* meter : meters)
			meter->setParameters (parameters);
	}
	static juce::String busName (int bus) { return juce::String (buses::name (bus)); }

	void paint (juce::Graphics& g) override;
	void resized() override;

private:
	juce::OwnedArray<LevelMeter> meters;
	juce::Rectangle<int> labelArea;
};

//==============================================================================
// Channel strip of one deck: a knob per stem, with mute and one switch per
// bus (1-4, AUX -- one stem per bus, the rest on AUX, Buses.h), above the
// channel fader.
class ChannelStrip : public juce::Component
{
public:
	// Where its volume fader stands, for the deck beside it to line up with.
	juce::Rectangle<int> faderBounds() const { return fader.getBounds(); }
	ChannelStrip (StemDeckPlayer& player, int deckIndex);

	void setStemNames (const std::array<juce::String, StemSet::numStems>& names);
	void toggleMute (int stem);
	void refresh(); // meters, called by the main timer
	void setMeterParameters (MeterParameters parameters) { meter.setParameters (parameters); }

	// The bus switches as the player has them, after a change from elsewhere
	// (the rule, or Core, spec stemdeck-remote); nothing is sent back from here.
	void showBuses (int stem);
	// A click on one of the stem's bus switches, asking for `on`. The mixer
	// applies it under the rule; the strip does not touch the player itself.
	std::function<void (int stem, int bus, bool on)> onBusSwitch;

	// The channel fader, moved as by hand (the Auto-DJ's crossfade).
	void setFaderDb (double db) { fader.setValue (db, juce::sendNotificationSync); }
	// ... and as a controller's slider does: 0..1 of its travel, with the
	// on-screen fader's own curve.
	void setFaderTravel (double t) { fader.setValue (fader.proportionOfLengthToValue (juce::jlimit (0.0, 1.0, t)), juce::sendNotificationSync); }
	double getFaderTravel() { return fader.valueToProportionOfLength (fader.getValue()); }
	bool isMuted (int stem) const { return muteButtons[stem]->getToggleState(); }

	// Knobs, mutes, bus switches and fader, for the session.
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
	// A deck's volume fader in this panel's coordinates.
	juce::Rectangle<int> faderArea (int deckIndex) const
	{
		return strip (deckIndex).faderBounds() + strip (deckIndex).getPosition();
	}
	void refresh();

	// One bus switch under the one-stem-per-bus rule (Buses.h), across both
	// decks: the players and the screen follow. Returns the stems Core must
	// hear (buses::stemIndex), the switched one always among them.
	std::vector<int> switchBus (int deck, int stem, int bus, bool on);
	// After a session load: brings both decks' switches into the rule.
	void normaliseBuses();
	// Called for every stem a click moved, the clicked one included.
	std::function<void (int deck, int stem)> onBusesChanged;

	void setOutputLevel (int channel, float peak) { outputMeters.setLevel (channel, peak); }
	// Every meter on it: both strips and the output meters.
	void setMeterParameters (MeterParameters parameters);

	void paint (juce::Graphics& g) override;
	void resized() override;

private:
	buses::Masks busMasks() const;
	void setBusMasks (const buses::Masks& masks);

	std::array<StemDeckPlayer*, buses::decks> players;
	ChannelStrip stripA, stripB;
	OutputMeters outputMeters;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerPanel)
};
