// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

#include "Remote.h"
#include "StemDeckPlayer.h"

#include <atomic>
#include <string>

// The one-deck instance's (-bformat --deck X) talk with Core, read from the
// one truth: a device hello every 30 s as the two-deck instance says it, the
// deck hello with the host every 2 s (remote::oneDeckSends), and the desk's
// 3D for this deck from Core into `deckThreeD`, applied as received. No bus
// switches, no reports, no stem meters (RemoteLink, two-deck). The deck's tempo
// goes to Core as `stemdeck.tempo` (remote::TempoReport) once followPlayer()
// names the deck. Without the
// truth or its words the link stays off or silent on them, and the deck's 3D
// stays where it was (1).
class DeckLink : private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>,
				 private juce::Timer
{
public:
	DeckLink (int oscDeck, std::atomic<float>& deckThreeD);
	~DeckLink() override;

	void start (const std::string& truthPath, const juce::String& truthHash);
	// The deck whose tempo is reported; `isMaster` is what it shows as MASTER.
	// Both outlive this link.
	void followPlayer (const StemDeckPlayer& player, std::function<bool()> isMaster);

private:
	void oscMessageReceived (const juce::OSCMessage& message) override;
	void timerCallback() override;
	void sayHello();
	void sendTempo();

	// The tempo is looked at every tick; the hello is every 20th (2 s).
	static constexpr int tickMs = 100, helloEveryTicks = 20, deviceHelloEvery = 15;   // hellos: 30 s

	const int oscDeck;
	std::atomic<float>& deckThreeD;
	remote::Words words;
	juce::String truthHash, host;
	juce::OSCReceiver receiver;
	juce::OSCSender toCore;
	int ticks = 0;
	const StemDeckPlayer* player = nullptr;
	std::function<bool()> isMaster;
	remote::TempoReport tempoReport;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeckLink)
};
