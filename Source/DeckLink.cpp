// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "DeckLink.h"

#include "OscTruthFile.h"
#include "PlayerTempo.h"

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
	if (const auto outside = remote::deckOutsideTruth (words, oscDeck); ! outside.empty())
		std::cerr << "StemDeck: " << outside << std::endl;
	const auto listenPort = remote::deckListenPort (endpoints.ownPort, oscDeck);
	if (! receiver.connect (listenPort))
	{
		std::cerr << "StemDeck: cannot listen on " << listenPort << " -- no 3D from Core" << std::endl;
		return;
	}
	receiver.addListener (this);
	toCore.connect (endpoints.coreHost, endpoints.corePort);

	sayHello();
	startTimer (tickMs);
}

void DeckLink::followPlayer (const StemDeckPlayer& followed, std::function<bool()> master)
{
	player = &followed;
	isMaster = std::move (master);
}

void DeckLink::sayHello()
{
	if (ticks % (helloEveryTicks * deviceHelloEvery) == 0)
		toCore.send (juce::OSCMessage (juce::String (words.hello), juce::String ("stemdeck"), truthHash));
	for (const auto& send : remote::oneDeckSends (words, oscDeck, host.toStdString()))
		toCore.send (juce::OSCMessage (juce::String (send.address), juce::String (send.text)));
}

// The report keeps its own clock (a change at once, else every 2 s); the hello
// above keeps the host and the device hello.
void DeckLink::sendTempo()
{
	if (player == nullptr || words.deckTempo.empty())
		return;
	const auto tempo = tempoReport.next (tempoOfPlayer (*player, isMaster && isMaster()),
										 juce::Time::getMillisecondCounterHiRes());
	if (! tempo)
		return;
	if (const auto send = remote::tempoMessage (words, oscDeck, *tempo))
		toCore.send (juce::OSCMessage (juce::String (send->address), send->bpm, (juce::int32) send->playing,
									   (juce::int32) send->master));
}

void DeckLink::timerCallback()
{
	++ticks;
	if (ticks % helloEveryTicks == 0)
		sayHello();
	sendTempo();
}

void DeckLink::oscMessageReceived (const juce::OSCMessage& message)
{
	if (message.size() != 1 || ! message[0].isFloat32())
		return;
	const auto address = message.getAddressPattern().toString().toStdString();
	if (const auto value = remote::parseDeckThreeD (words, address, oscDeck, message[0].getFloat32()))
		deckThreeD.store (*value, std::memory_order_relaxed);
}
