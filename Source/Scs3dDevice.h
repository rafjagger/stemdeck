#pragma once

#include <JuceHeader.h>
#include "Scs3d.h"

// One connected Stanton SCS.3d: its MIDI in and out, set up on open, its
// messages decoded (Scs3d.h) and handed to the message thread, its LEDs kept
// in step by show() -- only what changed is sent.
class Scs3dDevice : private juce::MidiInputCallback
{
public:
	Scs3dDevice (const juce::MidiDeviceInfo& input, const juce::MidiDeviceInfo& output);
	~Scs3dDevice() override;

	bool isOpen() const { return in != nullptr && out != nullptr; }
	juce::String getIdentifier() const { return inputInfo.identifier; }

	// On the message thread.
	std::function<void (const scs3d::Event&)> onEvent;

	void show (const scs3d::Leds& leds);

	// The SCS.3d devices connected now, in a stable order (by identifier),
	// each as its input and output.
	static std::vector<std::pair<juce::MidiDeviceInfo, juce::MidiDeviceInfo>> find();

private:
	void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message) override;

	juce::MidiDeviceInfo inputInfo;
	std::unique_ptr<juce::MidiInput> in;
	std::unique_ptr<juce::MidiOutput> out;
	std::map<int, std::uint8_t> sent;   // LED id -> value last sent
	std::shared_ptr<bool> alive = std::make_shared<bool> (true);

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Scs3dDevice)
};
