// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "RecorderControls.h"

#include "Theme.h"

namespace
{
// What the button says when a take ended by itself: a few characters that
// survive the 9 % slot, readable at a glance.
juce::String faceFor (Recorder::StopCause cause)
{
	switch (cause)
	{
		case Recorder::StopCause::writeFailed:  return "WRITE ERR";
		case Recorder::StopCause::limitReached: return "100 GB FULL";
		case Recorder::StopCause::targetGone:   return "STICK GONE";
		case Recorder::StopCause::none:         break;
	}
	return {};
}
}

RecorderControls::RecorderControls (Recorder& r) : recorder (r)
{
	button.setMouseClickGrabsKeyboardFocus (false);
	button.setColour (juce::TextButton::buttonOnColourId, Theme::mute);

	// Defaults ask the real mount table, but only once something calls them.
	localFolder = Recorder::defaultFolder().getFullPathName().toStdString();
	probe.state = [this] { return RecordTarget::stickState (stickMountPoint); };
	probe.canWriteNow = [this] { return RecordTarget::canWrite (stickMountPoint); };
	target = RecordTarget::choose (stickMountPoint, localFolder, RecordTarget::Stick::absent);
}

void RecorderControls::setTargets (std::string mountPoint, std::string local, StickProbe newProbe)
{
	stickMountPoint = std::move (mountPoint);
	localFolder = std::move (local);
	probe = std::move (newProbe);
	refreshTarget();
}

void RecorderControls::refreshTarget()
{
	target = RecordTarget::choose (stickMountPoint, localFolder, probe.state());
	lastAsked = clock();
	asked = true;
}

void RecorderControls::addTo (juce::Component& parent)
{
	for (auto* part : { (juce::Component*) &button, (juce::Component*) &left, (juce::Component*) &right })
		parent.addAndMakeVisible (part);
}

void RecorderControls::setMeterParameters (MeterParameters parameters)
{
	left.setParameters (parameters);
	right.setParameters (parameters);
}

void RecorderControls::showStopped (const juce::String& face, const juce::String& message)
{
	stoppedFace = face;
	stoppedMessage = message;
	stoppedAt = clock();
	button.setTooltip (message);
}

// During a take the mount table is read at most once a second.
bool RecorderControls::stickStillThere()
{
	if (clock() - lastStillThereAsk >= 1000)
	{
		stillThere = probe.state() != RecordTarget::Stick::absent;
		lastStillThereAsk = clock();
	}
	return stillThere;
}

void RecorderControls::toggle (double sampleRate)
{
	if (recorder.isRecording())
	{
		recorder.stop();
		lastTake = "Recorded: " + recorder.getFile().getFullPathName();
	}
	else
	{
		stoppedFace = stoppedMessage = {};
		refreshTarget();   // the stick may have come or gone since the last second
		// The one place that touches the mount point: a stick that mounts
		// read-only in fact is not a target.
		if (target.kind == RecordTarget::Kind::usb && ! probe.canWriteNow())
			target = RecordTarget::choose (stickMountPoint, localFolder, RecordTarget::Stick::readOnly);

		Recorder::TakeRules rules;
		if (target.kind == RecordTarget::Kind::local)
			rules.localLimitBytes = RecordTarget::localLimitBytes;
		else
		{
			stillThere = true;
			lastStillThereAsk = clock();
			rules.targetStillThere = [this] { return stickStillThere(); };
		}

		if (const auto error = recorder.start (juce::File (target.folder), sampleRate, rules); error.isNotEmpty())
		{
			// Follows a deliberate press: a box is fine here.
			showStopped ({}, error);
			juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Recording", error);
		}
	}
	update (jackClient);
}

juce::String RecorderControls::takeClock (int seconds)
{
	if (seconds >= 99 * 60 + 60)
		return juce::String (seconds / 3600) + ":" + juce::String (seconds / 60 % 60).paddedLeft ('0', 2) + "h";
	return juce::String (seconds / 60) + ":" + juce::String (seconds % 60).paddedLeft ('0', 2);
}

void RecorderControls::update (const juce::String& clientName)
{
	jackClient = clientName;
	left.setLevel (recorder.popPeak (0));
	right.setLevel (recorder.popPeak (1));

	// An automatic stop never covers the screen: a hand may be on the mixer.
	if (recorder.poll())
	{
		lastTake = "Recorded: " + recorder.getFile().getFullPathName();
		showStopped (faceFor (recorder.stopCause()), recorder.stopReason());
	}

	if (stoppedFace.isNotEmpty() && clock() - stoppedAt >= stoppedFaceMillis)
		stoppedFace = {};

	// Not while a take runs: the stick pulled then is poll()'s to report, and
	// the label must keep naming where this take goes.
	if (! recorder.isRecording() && (! asked || clock() - lastAsked >= 1000))
		refreshTarget();

	const auto on = recorder.isRecording();
	button.setToggleState (on, juce::dontSendNotification);
	if (on)
	{
		const auto seconds = (int) recorder.getSeconds();
		button.setButtonText (takeClock (seconds) + " " + target.label());
		button.setTooltip (recorder.getFile().getFullPathName()
						   + (recorder.hasDropped() ? juce::String ("\n! The disk fell behind: gaps in the recording") : juce::String()));
		button.removeColour (juce::TextButton::buttonColourId);
		// Gaps in the take: amber instead of red, since a mark in the text is the first thing a narrow slot loses.
		button.setColour (juce::TextButton::buttonOnColourId, recorder.hasDropped() ? Theme::cue : Theme::mute);
		return;
	}

	if (stoppedFace.isNotEmpty())
	{
		button.setButtonText (stoppedFace);
		button.setColour (juce::TextButton::buttonColourId, Theme::mute);
		button.setTooltip (stoppedMessage);
		return;
	}

	button.removeColour (juce::TextButton::buttonColourId);
	button.setButtonText ("REC " + juce::String (target.label()));
	if (stoppedMessage.isNotEmpty())
		button.setTooltip (stoppedMessage);
	else if (clientName.isEmpty())
		button.setTooltip (juce::String ("Recording needs JACK (inputs rec_L / rec_R)"));
	else if (lastTake.isNotEmpty())
		button.setTooltip (lastTake);
	else
		button.setTooltip ("Records the JACK inputs " + clientName + ":rec_L / rec_R as FLAC into " + target.folder);
}
