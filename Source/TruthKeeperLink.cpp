#include "TruthKeeperLink.h"

#include <iostream>

TruthKeeperLink::TruthKeeperLink (std::string ownIn, juce::File cacheIn,
								  std::function<juce::String (const juce::String&)> usableIn,
								  std::function<void()> restartIn)
	: own (std::move (ownIn)), cache (std::move (cacheIn)), usable (std::move (usableIn)),
	  restart (std::move (restartIn))
{
}

TruthKeeperLink::~TruthKeeperLink()
{
	stopTimer();
	receiver.removeListener (this);
	receiver.disconnect();
	fetcher.removeAllJobs (true, 6000);
}

bool TruthKeeperLink::start()
{
	// Shared with A3 Motion on this machine: both reuse the port, both hear it.
	socket.setEnablePortReuse (true);
	if (! socket.bindToPort (truthkeeper::announcePort) || ! receiver.connectToSocket (socket))
	{
		std::cerr << "StemDeck: cannot listen for Core's truth on " << truthkeeper::announcePort << std::endl;
		return false;
	}
	receiver.addListener (this);
	return true;
}

void TruthKeeperLink::startPolling (juce::String url)
{
	pollUrl = std::move (url);
	timerCallback();
	startTimer (truthkeeper::pollSeconds * 1000);
}

void TruthKeeperLink::timerCallback()
{
	if (busy.exchange (true))
		return;
	fetcher.addJob ([this, url = pollUrl] { take (url, {}); busy = false; });
}

void TruthKeeperLink::oscMessageReceived (const juce::OSCMessage& message)
{
	if (message.getAddressPattern().toString() != truthkeeper::announceAddress || message.size() != 2
		|| ! message[0].isString() || ! message[1].isString())
		return;
	const auto url = message[0].getString();
	const auto announced = message[1].getString();
	if (! truthkeeper::needsFetch (announced.toStdString(), own) || busy.exchange (true))
		return;
	fetcher.addJob ([this, url, announced] { take (url, announced); busy = false; });
}

void TruthKeeperLink::take (juce::String url, juce::String announced)
{
	juce::StringPairArray headers;
	int status = 0;
	auto stream = juce::URL (url).createInputStream (
		juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
			.withConnectionTimeoutMs (5000)
			.withResponseHeaders (&headers)
			.withStatusCode (&status));
	if (stream == nullptr || status != 200)
	{
		refuse ("fetch failed (status " + juce::String (status) + ")");
		return;
	}
	refusals.answered();
	// Polled, there is no announcement: the header says which truth this is,
	// and every 30 s it is the one StemDeck already holds.
	const auto header = headers["X-A3-Truth"].toStdString();
	const auto expected = truthkeeper::announcedOr (announced.toStdString(), header);
	if (! truthkeeper::needsFetch (expected, own))
		return;
	juce::MemoryBlock body;
	stream->readIntoMemoryBlock (body);
	const auto hash = juce::SHA256 (body).toHexString();
	if (! truthkeeper::verified (hash.toStdString(), header, expected))
	{
		refuse ("body, header and announcement do not agree");
		return;
	}
	const auto text = body.toString();
	if (const auto why = usable (text); why.isNotEmpty())
	{
		refuse (why);
		return;
	}
	cache.getParentDirectory().createDirectory();
	juce::TemporaryFile temporary (cache);
	if (! temporary.getFile().replaceWithData (body.getData(), body.getSize())
		|| ! temporary.overwriteTargetFileWithTemporary())
	{
		refuse ("cannot write " + cache.getFullPathName());
		return;
	}
	std::cerr << "StemDeck: Core announced another truth, restarting on it" << std::endl;
	juce::MessageManager::callAsync (restart);
}

void TruthKeeperLink::refuse (const juce::String& reason)
{
	// Said once per reason, not every 2 s while Core keeps announcing it.
	if (! refusals.shouldSay (reason.toStdString()))
		return;
	std::cerr << "StemDeck: Core's truth refused: " << reason << std::endl;
}
