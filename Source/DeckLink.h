// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

#include "Remote.h"

#include <atomic>
#include <string>

// The one-deck instance's (-bformat --deck X) talk with Core, read from the
// one truth: a device hello every 30 s as the two-deck instance says it, the
// deck hello with the host every 2 s (remote::oneDeckSends), and the desk's
// 3D for this deck from Core into `deckThreeD`, applied as received. No bus
// switches, no reports, no stem meters (RemoteLink, two-deck). Without the
// truth or its words the link stays off or silent on them, and the deck's 3D
// stays where it was (1).
class DeckLink : private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>,
				 private juce::Timer
{
public:
	DeckLink (int oscDeck, std::atomic<float>& deckThreeD);
	~DeckLink() override;

	void start (const std::string& truthPath, const juce::String& truthHash);

private:
	void oscMessageReceived (const juce::OSCMessage& message) override;
	void timerCallback() override;
	void sayHello();

	static constexpr int helloEveryMs = 2000, deviceHelloEvery = 15;   // ticks: 30 s

	const int oscDeck;
	std::atomic<float>& deckThreeD;
	remote::Words words;
	juce::String truthHash, host;
	juce::OSCReceiver receiver;
	juce::OSCSender toCore;
	int ticks = 0;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeckLink)
};
