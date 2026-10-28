// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include "JackOutput.h"

// Where StemDeck's audio goes, for both screens: a JACK client with named
// ports (and the recorder's inputs) when a JACK server runs, otherwise a
// regular audio device, remembered in the settings ("audioDeviceState").
class AudioOutput : private juce::ChangeListener
{
public:
	explicit AudioOutput (juce::PropertiesFile& settings);
	~AudioOutput() override;

	// JACK first; without it the device, with as many outputs as there are ports.
	void open (const juce::String& clientName, const juce::StringArray& portNames, juce::AudioSource& source,
	           const juce::StringArray& inputNames, JackOutput::InputSink* inputSink);
	void closeJack();   // no more input blocks
	// Under JACK: makes and keeps the map's links (JackOutput::keepLinks); without JACK nothing.
	void keepLinks (a3::links::LinkMap map) { if (jackOpen) jack.keepLinks (std::move (map)); }
	void close();

	bool usingJack() const { return jackOpen; }
	const JackOutput& jackClient() const { return jack; }

	// Message thread. Under JACK: output port `index` has a connection.
	bool isPortConnected (int index) const { return jackOpen && jack.isPortConnected (index); }
	// Without JACK: the device's active outputs (0: none open).
	int activeDeviceOutputs() const;

	// The device line in the top bar; `wantedDeviceOutputs`: fewer active
	// device outputs than these is "summed down".
	enum class Health { fine, warning, failed };
	struct Status { juce::String text; Health health = Health::fine; };
	Status status (int wantedDeviceOutputs) const;
	// The line's colour, and the line with why there are no spatial outputs (empty: none missing).
	static juce::Colour colourOf (Health);
	static juce::String withSpatialError (const juce::String& text, const juce::String& spatialError);

	void showDeviceSettings();
	std::function<void()> onDeviceChanged;

private:
	void openDevice (int numOutputs, juce::AudioSource& source);
	void changeListenerCallback (juce::ChangeBroadcaster*) override;

	juce::PropertiesFile& settings;
	JackOutput jack;
	bool jackOpen = false;
	int deviceOutputs = 0;

	// Fallback when no JACK server is running.
	juce::AudioDeviceManager deviceManager;
	juce::AudioSourcePlayer audioSourcePlayer;
};
