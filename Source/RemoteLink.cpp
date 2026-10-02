#include "RemoteLink.h"

#include "Buses.h"
#include "OscTruthFile.h"

#include <iostream>

RemoteLink::RemoteLink (std::array<StemDeckPlayer*, 2> p, MixerPanel& m)
	: players (p), mixer (m)
{
}

RemoteLink::~RemoteLink()
{
	stopTimer();
	receiver.removeListener (this);
	receiver.disconnect();
}

void RemoteLink::start (const std::string& path, const juce::String& hash)
{
	std::string error;
	const auto listeners = osctruth::readListeners (path, error);
	const auto hosts = error.empty() ? osctruth::readHosts (path, error) : std::map<std::string, std::string>();
	words = error.empty() ? osctruth::readRemoteWords (path, error) : remote::Words();
	const auto endpoints = osctruth::endpointsFrom (listeners, hosts);
	truthHash = hash;

	if (! error.empty() || ! endpoints.complete())
	{
		std::cerr << "StemDeck: no remote control from the desk -- "
				  << (error.empty() ? std::string ("the truth names no core, mixer or stemdeck osc") : error) << std::endl;
		return;
	}

	if (! receiver.connect (endpoints.ownPort))
	{
		std::cerr << "StemDeck: cannot listen on " << endpoints.ownPort << " -- no remote control" << std::endl;
		return;
	}
	receiver.addListener (this);
	toCore.connect (endpoints.coreHost, endpoints.corePort);
	toDesk.connect (endpoints.mixerHost, endpoints.mixerPort);

	running = true;
	sayHello();
	startTimerHz (levelsPerSecond);
}

void RemoteLink::report (int deck, int stem)
{
	if (! running)
		return;
	std::array<bool, 6> on {};
	for (int bus = 0; bus < buses::count; ++bus)
		on[(size_t) bus] = players[(size_t) deck]->isStemOnBus (stem, bus);
	toCore.send (juce::OSCMessage (juce::String (remote::reportAddress (words, deck, stem)),
								   (juce::int32) remote::maskOf (on)));
}

void RemoteLink::reportAll()
{
	for (int deck = 0; deck < 2; ++deck)
		for (int stem = 0; stem < StemSet::numStems; ++stem)
			report (deck, stem);
}

void RemoteLink::oscMessageReceived (const juce::OSCMessage& message)
{
	const auto address = message.getAddressPattern().toString().toStdString();
	if (remote::isRecall (words, address))
	{
		reportAll();
		return;
	}
	if (message.size() != 1 || ! message[0].isInt32())
		return;
	const auto command = remote::parseSwitch (words, address, message[0].getInt32());
	if (! command)
		return;

	// The same switch a click sets; the screen follows, then Core hears it.
	players[(size_t) command->deck]->setStemOnBus (command->stem, command->bus, command->on);
	mixer.strip (command->deck).showBuses (command->stem);
	report (command->deck, command->stem);
}

void RemoteLink::timerCallback()
{
	sendLevels();
	if (++ticks % (levelsPerSecond * helloEverySeconds) == 0)
		sayHello();
}

void RemoteLink::sayHello()
{
	toCore.send (juce::OSCMessage (juce::String (words.hello), juce::String ("stemdeck"), truthHash));
}

void RemoteLink::sendLevels()
{
	for (int deck = 0; deck < 2; ++deck)
		for (int stem = 0; stem < StemSet::numStems; ++stem)
		{
			const auto level = players[(size_t) deck]->popDeskLevel (stem);
			toDesk.send (juce::OSCMessage (juce::String (remote::vuAddress (words, deck, stem)), level.peak, level.rms));
		}
}
