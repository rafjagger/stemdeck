// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "RecorderControls.h"

#include "Theme.h"

RecorderControls::RecorderControls (Recorder& r) : recorder (r)
{
	button.setMouseClickGrabsKeyboardFocus (false);
	button.setColour (juce::TextButton::buttonOnColourId, Theme::mute);
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

void RecorderControls::toggle (double sampleRate)
{
	if (recorder.isRecording())
	{
		recorder.stop();
		button.setTooltip ("Aufgenommen: " + recorder.getFile().getFullPathName());
	}
	else if (const auto error = recorder.start (Recorder::defaultFolder(), sampleRate); error.isNotEmpty())
	{
		juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Recording", error);
	}
	update (jackClient);
}

void RecorderControls::update (const juce::String& clientName)
{
	jackClient = clientName;
	left.setLevel (recorder.popPeak (0));
	right.setLevel (recorder.popPeak (1));

	const auto on = recorder.isRecording();
	button.setToggleState (on, juce::dontSendNotification);
	if (on)
	{
		const auto seconds = (int) recorder.getSeconds();
		button.setButtonText (juce::String::fromUTF8 ("\xe2\x97\x8f ") + juce::String (seconds / 60) + ":" + juce::String (seconds % 60).paddedLeft ('0', 2)
							  + (recorder.hasDropped() ? " !" : ""));
		button.setTooltip (recorder.getFile().getFullPathName()
						   + (recorder.hasDropped() ? juce::String ("\n! The disk fell behind: gaps in the recording") : juce::String()));
	}
	else
	{
		button.setButtonText ("REC");
		if (clientName.isEmpty())
			button.setTooltip (juce::String ("Recording needs JACK (inputs rec_L / rec_R)"));
		else if (button.getTooltip().isEmpty())
			button.setTooltip ("Records the JACK inputs " + clientName + ":rec_L / rec_R as FLAC into "
							   + Recorder::defaultFolder().getFullPathName());
	}
}
