#pragma once

#include <JuceHeader.h>

// The audio device selector with a Close button under it: i3 gives a floating
// window no close button, so the touch screen had no way out (2026-10-06).
// Device changes apply as they are made, so Close is all it needs.
class AudioPanel : public juce::Component
{
public:
	AudioPanel (juce::AudioDeviceManager& deviceManager, int outputChannels);

	void resized() override;

private:
	juce::AudioDeviceSelectorComponent selector;
	juce::TextButton closeButton { "Close" };
};
