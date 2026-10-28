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

	// Where takes go: the stick's mount point, the local folder, and the two
	// questions about the stick. `state` reads the mount table (asked once a
	// second); `canWriteNow` touches the mount point and is asked only when a
	// take starts. The defaults are the package's (/media/a3-usb, the data
	// folder, /proc/mounts); tests inject, and nothing is asked until then.
	struct StickProbe
	{
		std::function<RecordTarget::Stick()> state;
		std::function<bool()> canWriteNow;
	};
	void setTargets (std::string mountPoint, std::string localFolder, StickProbe probe);
	void setClock (std::function<juce::uint32()> millis) { clock = std::move (millis); }
	void refreshTarget();   // asks the mount table now; update() asks it once a second

	// A message after a take that did not run to its end: the button face names
	// it for a while (no popup, a hand may be on the mixer), the tooltip says it all.
	void showStopped (const juce::String& face, const juce::String& message);

	static constexpr juce::uint32 stoppedFaceMillis = 20000;
	// m:ss, and h:mmh past 99 minutes: never wider than five characters.
	static juce::String takeClock (int seconds);
	// What the button says when a take ended by itself: a few characters that
	// survive the 9 % slot, readable at a glance. Empty for an ordinary stop.
	static juce::String stopFace (Recorder::StopCause cause);

	juce::TextButton button { "REC" };
	LevelMeter left, right;

private:
	bool stickStillThere();

	Recorder& recorder;
	juce::String jackClient;
	std::string stickMountPoint = "/media/a3-usb", localFolder;
	StickProbe probe;
	std::function<juce::uint32()> clock { [] { return juce::Time::getMillisecondCounter(); } };
	RecordTarget target;
	juce::String stoppedFace, stoppedMessage, lastTake;
	juce::uint32 lastAsked = 0, stoppedAt = 0, lastStillThereAsk = 0;
	bool asked = false, stillThere = true;
};
