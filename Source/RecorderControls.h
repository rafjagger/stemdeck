// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include "MixerPanel.h"
#include "Recorder.h"

// REC in the top bar, with the two JACK inputs' levels beside it: the
// recorder's face, the same on both screens. The screen adds and places the
// three parts; recording needs JACK (rec_L / rec_R).
class RecorderControls
{
public:
	explicit RecorderControls (Recorder&);

	void addTo (juce::Component& parent);
	void setMeterParameters (MeterParameters);

	// Message thread, every tick: the levels, the running time, the tooltip.
	// `clientName`: whose rec_L / rec_R; empty without JACK.
	void update (const juce::String& clientName);
	void toggle (double sampleRate);

	juce::TextButton button { "REC" };
	LevelMeter left, right;

private:
	Recorder& recorder;
	juce::String jackClient;
};
