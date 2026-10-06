#include "AudioPanel.h"

AudioPanel::AudioPanel (juce::AudioDeviceManager& deviceManager, int outputChannels)
	: selector (deviceManager, 0, 0, 2, outputChannels, false, false, true, false)
{
	addAndMakeVisible (selector);

	closeButton.onClick = [this]
	{
		if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
			dialog->exitModalState (0);
	};
	addAndMakeVisible (closeButton);

	setSize (520, 506);
}

void AudioPanel::resized()
{
	auto area = getLocalBounds();
	auto buttons = area.removeFromBottom (46).reduced (12, 6);
	closeButton.setBounds (buttons.removeFromRight (110));
	selector.setBounds (area);
}
