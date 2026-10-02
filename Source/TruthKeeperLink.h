#pragma once

#include <JuceHeader.h>

#include "TruthKeeper.h"

#include <functional>
#include <string>

// The JUCE side of the truth keeper (TruthKeeper.h has the decisions): hears
// /core/here on the announce port, fetches a fingerprint StemDeck does not
// have off the message thread, checks it, writes the cache whole and asks the
// app to restart. A refusal is said once per reason; StemDeck runs on.
//
// The port is shared with A3 Motion on the same machine: both bind it with
// port reuse, and both receive the broadcast.
class TruthKeeperLink : private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>
{
public:
	// `usable(body)` returns why a fetched truth cannot be used, or empty.
	TruthKeeperLink (std::string own, juce::File cache,
					 std::function<juce::String (const juce::String& body)> usable,
					 std::function<void()> restart);
	~TruthKeeperLink() override;

	bool start();

private:
	void oscMessageReceived (const juce::OSCMessage& message) override;
	void take (juce::String url, juce::String announced);
	void refuse (const juce::String& reason);

	std::string own;
	juce::File cache;
	std::function<juce::String (const juce::String&)> usable;
	std::function<void()> restart;
	juce::DatagramSocket socket { true };
	juce::OSCReceiver receiver;
	juce::ThreadPool fetcher { juce::ThreadPoolOptions{}.withNumberOfThreads (1) };
	std::atomic<bool> busy { false };
	juce::String lastReason;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TruthKeeperLink)
};
