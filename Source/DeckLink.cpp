// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "DeckLink.h"

#include "OscTruthFile.h"

#include <iostream>

DeckLink::DeckLink (int deck, std::atomic<float>& threeD)
	: oscDeck (deck), deckThreeD (threeD)
{
}

DeckLink::~DeckLink()
{
	stopTimer();
	receiver.removeListener (this);
	receiver.disconnect();
}

void DeckLink::start (const std::string& path, const juce::String& hash)
{
	std::string error;
	const auto listeners = osctruth::readListeners (path, error);
	const auto hosts = error.empty() ? osctruth::readHosts (path, error) : std::map<std::string, std::string>();
	words = error.empty() ? osctruth::readRemoteWords (path, error) : remote::Words();
	const auto endpoints = osctruth::endpointsFrom (listeners, hosts);
	truthHash = hash;
	host = juce::SystemStats::getComputerName();

	if (! error.empty() || ! endpoints.complete())
	{
		std::cerr << "StemDeck: no link to Core -- "
				  << (error.empty() ? std::string ("the truth names no core, mixer or stemdeck osc") : error) << std::endl;
		return;
	}
	if (! receiver.connect (endpoints.ownPort))
	{
		std::cerr << "StemDeck: cannot listen on " << endpoints.ownPort << " -- no 3D from Core" << std::endl;
		return;
	}
	receiver.addListener (this);
	toCore.connect (endpoints.coreHost, endpoints.corePort);

	sayHello();
	startTimer (helloEveryMs);
}

void DeckLink::sayHello()
{
	if (ticks % deviceHelloEvery == 0)
		toCore.send (juce::OSCMessage (juce::String (words.hello), juce::String ("stemdeck"), truthHash));
	for (const auto& send : remote::oneDeckSends (words, oscDeck, host.toStdString()))
		toCore.send (juce::OSCMessage (juce::String (send.address), juce::String (send.text)));
}

void DeckLink::timerCallback()
{
	++ticks;
	sayHello();
}

void DeckLink::oscMessageReceived (const juce::OSCMessage& message)
{
	if (message.size() != 1 || ! message[0].isFloat32())
		return;
	const auto address = message.getAddressPattern().toString().toStdString();
	if (const auto value = remote::parseDeckThreeD (words, address, oscDeck, message[0].getFloat32()))
		deckThreeD.store (*value, std::memory_order_relaxed);
}
