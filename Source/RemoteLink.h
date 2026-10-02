#pragma once

#include <JuceHeader.h>

#include "MixerPanel.h"
#include "OscTruth.h"
#include "Remote.h"
#include "StemDeckPlayer.h"

#include <array>

// StemDeck by remote control from the A3 Mixer, through Core (spec
// stemdeck-remote, 2026-10-01). StemDeck owns its bus switches: Core sets
// them with /stemdeck/{deck}/{stem}/bus/{bus}, and every change -- from Core
// or from a click here -- goes back as the stem's mask. A hello every 30 s
// tells Core where StemDeck is; one level meter per stem goes to the desk at
// 25 Hz. Everything is read from the one truth; without it, or with words
// missing there, the link stays off and StemDeck plays on as before.
class RemoteLink : private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>,
				   private juce::Timer
{
public:
	RemoteLink (std::array<StemDeckPlayer*, 2> players, MixerPanel& mixer);
	~RemoteLink() override;

	void start();
	// A stem's switches to Core, after any change. Ignored while off.
	void report (int deck, int stem);
	// The sha256 of the truth StemDeck read: Core's fingerprint when it is
	// the body Core served.
	juce::String getTruthHash() const { return truthHash; }

private:
	void oscMessageReceived (const juce::OSCMessage& message) override;
	void timerCallback() override;
	void sayHello();
	void sendLevels();
	void reportAll();

	static constexpr int levelsPerSecond = 25;
	static constexpr int helloEverySeconds = 30;

	std::array<StemDeckPlayer*, 2> players;
	MixerPanel& mixer;
	remote::Words words;
	juce::String truthHash;
	juce::OSCReceiver receiver;
	juce::OSCSender toCore, toDesk;
	bool running = false;
	int ticks = 0;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RemoteLink)
};
