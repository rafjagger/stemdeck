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
	// The bar's normal range in this colour (a stem's) instead of the LEDs' green.
	void setBarColour (juce::Colour colour) { barColour = colour; repaint(); }
	void paint (juce::Graphics& g) override;

private:
	float secondsSinceLastFeed();

	PeakMeter ballistics;
	double lastFeedMs = 0.0;
	int litSegments = 0, heldSegment = 0;
	bool showsClip = false;
	std::optional<juce::Colour> barColour;
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

	// On its own tile: the bars and captions this far inside its bounds.
	void setInset (int newInset) { inset = newInset; resized(); }

	void paint (juce::Graphics& g) override;
	void resized() override;

private:
	juce::OwnedArray<LevelMeter> meters;
	juce::Rectangle<int> labelArea;
	int inset = 0;
};

//==============================================================================
// Channel strip of one deck: a knob per stem, with one switch per bus (1-4,
// AUX -- one stem per bus, the rest on AUX, Buses.h) and mute in one row; the
// four rows tight, a matrix of keys. The stems' names are on the overview's
// lanes, not here.
// Its channel fader, which shows the deck's level in its slot, stands in the
// band under the mixer: the strip owns it, the window places it.
class ChannelStrip : public juce::Component
{
public:
	ChannelStrip (StemDeckPlayer& player, int deckIndex);

	void toggleMute (int stem);
	void refresh(); // meters, called by the main timer
	void setMeterParameters (MeterParameters parameters)
	{
		for (auto* meter : stemMeters)
			meter->setParameters (parameters);
	}

	// The bus switches as the player has them, after a change from elsewhere
	// (the rule, or Core, spec stemdeck-remote); nothing is sent back from here.
	void showBuses (int stem);
	// A click on one of the stem's bus switches, asking for `on`. The mixer
	// applies it under the rule; the strip does not touch the player itself.
	std::function<void (int stem, int bus, bool on)> onBusSwitch;

	// The channel fader, moved as by hand (the Auto-DJ: unity on a load).
	void setFaderDb (double db) { fader.setValue (db, juce::sendNotificationSync); }
	// ... and as a controller's slider does: 0..1 of its travel, with the
	// on-screen fader's own curve.
	void setFaderTravel (double t) { fader.setValue (fader.proportionOfLengthToValue (juce::jlimit (0.0, 1.0, t)), juce::sendNotificationSync); }
	double getFaderTravel() { return fader.valueToProportionOfLength (fader.getValue()); }
	bool isMuted (int stem) const { return muteButtons[stem]->getToggleState(); }
	// A controller's encoder on a stem's knob: `detents` steps of a fixed
	// share of its travel, on the knob's own curve.
	void nudgeStemGain (int stem, double detents);

	// Knobs, mutes, bus switches and fader, for the session.
	void saveState (DeckSession& state) const;
	void restoreState (const DeckSession& state);

	// The fader and the stem meters behind it: made children of `parent`, placed by it.
	void addFaderTo (juce::Component& parent);
	void setFaderBounds (juce::Rectangle<int> area);

	void paint (juce::Graphics& g) override;
	void resized() override;

private:
	StemDeckPlayer& player;
	const int deckIndex;

	juce::OwnedArray<juce::Slider> knobs;
	juce::OwnedArray<juce::TextButton> muteButtons, busButtons;   // busButtons: stem * buses::count + bus
	juce::Slider fader { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
	juce::OwnedArray<LevelMeter> stemMeters;   // behind the fader, one per stem

	juce::Rectangle<int> headerArea;

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

	// One bus switch under the one-stem-per-bus rule (Buses.h), across both
	// decks: the players and the screen follow. Returns the stems Core must
	// hear (buses::stemIndex), the switched one always among them.
	std::vector<int> switchBus (int deck, int stem, int bus, bool on);
	// Where a switch sends a stem it takes off its channel: AUX, or off
	// while the Auto-DJ plays (Buses.h).
	void setSpare (buses::Spare s) { spare = s; }
	// The Auto-DJ's routing (buses::route), applied as a switch is: the
	// players and the screen follow, and every stem it moved is reported
	// through onBusesChanged, as after a click.
	void route (const std::vector<buses::Route>& routes);
	// A controller's key for one bus switch: exactly what a click on the
	// screen key does -- the switch toggles, the rule decides, Core hears.
	void pressBus (int deck, int stem, int bus);
	// After a session load: brings both decks' switches into the rule.
	void normaliseBuses();
	// Called for every stem a click moved, the clicked one included.
	std::function<void (int deck, int stem)> onBusesChanged;
	// Called for every switch a DJ made (switchBus: a click, the desk, the
	// panel), after it was applied -- not for the Auto-DJ's route().
	std::function<void (int deck, int stem, int bus, bool on)> onDjSwitch;

	// The volume faders and the output meters, in the band under the mixer:
	// made children of `parent`, placed by it.
	void addBandPartsTo (juce::Component& parent);
	// `metersTile` with the bars and captions `inset` inside it.
	void setBandBounds (juce::Rectangle<int> faderA, juce::Rectangle<int> faderB, juce::Rectangle<int> metersTile, int inset);

	void setOutputLevel (int channel, float peak) { outputMeters.setLevel (channel, peak); }
	// Every meter on it: both strips and the output meters.
	void setMeterParameters (MeterParameters parameters);

	void paint (juce::Graphics& g) override;
	void resized() override;

private:
	void busSwitched (int deck, int stem, int bus, bool on);
	buses::Masks busMasks() const;
	void setBusMasks (const buses::Masks& masks);

	std::array<StemDeckPlayer*, buses::decks> players;
	buses::Spare spare = buses::Spare::toAux;
	ChannelStrip stripA, stripB;
	OutputMeters outputMeters;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerPanel)
};
