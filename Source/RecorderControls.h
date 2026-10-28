// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include "LevelMeter.h"
#include "Recorder.h"
#include "RecordTarget.h"
#include <functional>

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

	// Where takes go: the stick's mount point, the local folder, and the
	// question "is the stick mounted and writable". The defaults are the
	// package's (/media/a3-usb, the data folder, /proc/mounts); tests inject.
	void setTargets (std::string mountPoint, std::string localFolder, std::function<bool()> stickIsThere);
	void refreshTarget();   // asks the stick question now; update() asks it once a second
	// The take ended by itself: the button says so until the next take starts.
	void showStopped (const juce::String& message);

	juce::TextButton button { "REC" };
	LevelMeter left, right;

private:
	RecordTarget currentTarget() const;
	juce::String faceText (const juce::String& lead) const;

	Recorder& recorder;
	juce::String jackClient;
	std::string stickMountPoint = "/media/a3-usb", localFolder;
	std::function<bool()> stickIsThere;
	RecordTarget target;
	juce::String stoppedMessage, lastTake;
	juce::uint32 lastAsked = 0;
};
