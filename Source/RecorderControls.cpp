// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "RecorderControls.h"

#include "Theme.h"

RecorderControls::RecorderControls (Recorder& r) : recorder (r)
{
	button.setMouseClickGrabsKeyboardFocus (false);
	button.setColour (juce::TextButton::buttonOnColourId, Theme::mute);
	setTargets ("/media/a3-usb", Recorder::defaultFolder().getFullPathName().toStdString(),
				[this] { return RecordTarget::isMountedAndWritable (stickMountPoint); });
}

void RecorderControls::setTargets (std::string mountPoint, std::string local, std::function<bool()> isThere)
{
	stickMountPoint = std::move (mountPoint);
	localFolder = std::move (local);
	stickIsThere = std::move (isThere);
	refreshTarget();
}

void RecorderControls::refreshTarget()
{
	target = RecordTarget::choose (stickMountPoint, localFolder, stickIsThere());
	lastAsked = juce::Time::getMillisecondCounter();
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

void RecorderControls::showStopped (const juce::String& message)
{
	stoppedMessage = message;
	button.setTooltip (message);
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
		stoppedMessage = {};
		refreshTarget();   // the stick may have come or gone since the last second
		Recorder::TakeRules rules;
		if (target.kind == RecordTarget::Kind::local)
			rules.localLimitBytes = RecordTarget::localLimitBytes;
		else
			rules.targetStillThere = stickIsThere;

		if (const auto error = recorder.start (juce::File (target.folder), sampleRate, rules); error.isNotEmpty())
		{
			showStopped (error);
			juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Recording", error);
		}
	}
	update (jackClient);
}

juce::String RecorderControls::faceText (const juce::String& lead) const
{
	return lead + target.label() + (stoppedMessage.isNotEmpty() ? " !" : "");
}

void RecorderControls::update (const juce::String& clientName)
{
	jackClient = clientName;
	left.setLevel (recorder.popPeak (0));
	right.setLevel (recorder.popPeak (1));

	if (recorder.poll())
	{
		lastTake = "Recorded: " + recorder.getFile().getFullPathName();
		showStopped (recorder.stopReason());
		juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Recording stopped", recorder.stopReason());
	}

	// Not while a take runs: the stick pulled then is poll()'s to report, and
	// the label must keep naming where this take goes.
	if (! recorder.isRecording() && juce::Time::getMillisecondCounter() - lastAsked >= 1000)
		refreshTarget();

	const auto on = recorder.isRecording();
	button.setToggleState (on, juce::dontSendNotification);
	if (on)
	{
		const auto seconds = (int) recorder.getSeconds();
		button.setButtonText (juce::String::fromUTF8 ("\xe2\x97\x8f ") + juce::String (seconds / 60) + ":" + juce::String (seconds % 60).paddedLeft ('0', 2)
							  + " " + target.label() + (recorder.hasDropped() ? " !" : ""));
		button.setTooltip (recorder.getFile().getFullPathName()
						   + (recorder.hasDropped() ? juce::String ("\n! The disk fell behind: gaps in the recording") : juce::String()));
	}
	else
	{
		button.setButtonText (faceText ("REC "));
		if (stoppedMessage.isNotEmpty())
			button.setTooltip (stoppedMessage);
		else if (clientName.isEmpty())
			button.setTooltip (juce::String ("Recording needs JACK (inputs rec_L / rec_R)"));
		else if (lastTake.isNotEmpty())
			button.setTooltip (lastTake);
		else
			button.setTooltip ("Records the JACK inputs " + clientName + ":rec_L / rec_R as FLAC into " + target.folder);
	}
}
