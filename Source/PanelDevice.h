#pragma once

#include <JuceHeader.h>
#include "PanelLink.h"
#include "PanelMap.h"

#include <atomic>
#include <mutex>

// The A³ Motion panel PCB on a USB serial port, driven by StemDeck where
// A³ Motion does not run. What its keys mean and what it shows is PanelMap's;
// this is only the port.
//
// Its own thread owns the port: the opening (a board reset and a PING
// handshake take seconds), every poll and every LED write. The message
// thread never waits on the panel -- it gets the decoded events posted to
// it, and hands over the LEDs it wants under a lock only the two of them
// take. The audio thread never comes near it.
//
// The port is found by the panel's USB ID only (PanelLink), and taken
// exclusively (flock and TIOCEXCL): A³ Motion and StemDeck can never both
// drive it -- whichever opened it first keeps it. With no panel, StemDeck
// runs as without one and looks again every two seconds.
class PanelDevice : private juce::Thread
{
public:
	PanelDevice();
	~PanelDevice() override;

	// On the message thread, one call per event.
	std::function<void (const panel::Event&)> onEvent;

	// From the message thread: what the LEDs should show. Only what changed
	// goes over the wire.
	void show (const panel::LedState& state);

	bool isConnected() const { return connected.load(); }

private:
	void run() override;

	bool open (const std::string& path);
	bool pingAnswers();
	void close();
	// One poll: false when the port has gone and was closed.
	bool poll();
	bool writeAll (const std::uint8_t* data, size_t size);
	bool readExact (std::uint8_t* data, size_t size, int timeoutMs);
	void writeLeds();
	void post (std::vector<panel::Event> events);
	void note (const juce::String& line);

	int fd = -1;
	std::string openPath;
	juce::String lastNote;
	panel::Reconnect reconnect;
	panel::InputDecoder decoder;
	int cycle = 0;

	std::mutex ledLock;
	panel::Leds wanted {};   // guarded by ledLock
	panel::Leds shown {};    // the thread's: what the panel shows
	bool shownKnown = false;

	std::atomic<bool> connected { false };
	std::shared_ptr<bool> alive = std::make_shared<bool> (true);

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PanelDevice)
};
