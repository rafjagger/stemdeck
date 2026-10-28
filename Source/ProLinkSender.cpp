#include "ProLinkSender.h"

#include "BeatScheduler.h"
#include "FollowLeader.h"
#include "StemDeckPlayer.h"

#include <JuceHeader.h>

#include <arpa/inet.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
	// A CDJ's status interval; the CDJ-3000's absolute position interval.
	constexpr double statusEvery = 0.2, positionEvery = 0.03;

	double now() { return juce::Time::getMillisecondCounterHiRes() / 1000.0; }

	// A deck as heard at `t`: its read head carried to now and delayed by the
	// audio still in flight.
	struct DeckNow
	{
		BeatGrid grid;
		bool loaded = false, playing = false, moving = false;
		double rate = 0.0, speed = 1.0, position = 0.0, length = 0.0;
	};

	DeckNow deckAt (const StemDeckPlayer& player, double t, double pathLatency)
	{
		DeckNow deck;
		deck.grid = player.getBeatGrid();
		deck.loaded = player.isLoaded();
		deck.playing = player.isPlaying();
		deck.moving = deck.playing && ! player.isScratching();
		deck.rate = player.getEffectiveRate();
		deck.speed = player.getSpeed();
		deck.length = player.getLength();
		deck.position = audiblePosition (positionAt (player.getPosition(), player.getPositionStamp(), t, deck.rate, deck.moving),
										 pathLatency, deck.moving ? deck.rate : 0.0);
		return deck;
	}

	// The status's view of the deck: no grid is no track -- nothing a player
	// could sync to.
	prolink::PlayerState playerState (const DeckNow& deck, bool synced)
	{
		prolink::PlayerState state;
		state.hasTrack = deck.grid.isValid();
		state.synced = synced;
		if (! state.hasTrack)
			return state;

		state.bpm = deck.grid.bpm;
		state.pitch = deck.speed;
		state.playing = deck.moving;
		// Where we are in the bar: the beat last passed. The lead-in before
		// the first downbeat counts into the bar, not the beat number.
		if (const auto passed = beatPassed (deck.grid.firstBeat, deck.grid.bpm, deck.position))
		{
			state.beatNumber = passed->index >= 0 ? (uint32_t) (passed->index + 1) : 0;
			state.beatInBar = passed->beatInBar;
		}
		return state;
	}

	prolink::Position positionOf (const DeckNow& deck)
	{
		prolink::Position position;
		position.trackSeconds = deck.length;
		position.playheadSeconds = std::max (0.0, deck.position);
		position.pitch = deck.speed;
		position.effectiveBpm = deck.grid.isValid() ? deck.grid.bpm * deck.rate : 0.0;
		return position;
	}

	void sleepFor (double seconds)
	{
		if (seconds > 0.0)
			std::this_thread::sleep_for (std::chrono::duration<double> (seconds));
	}
}

ProLinkSender::~ProLinkSender()
{
	stop();
}

bool ProLinkSender::start (int number, uint32_t broadcastIpNetworkOrder,
						   const osctruth::ProLinkPorts& truthPorts, const std::string& deviceName,
						   ProLinkRole* linkRole)
{
	stop();
	deviceNumber = number;
	broadcastIp = broadcastIpNetworkOrder;
	ports = truthPorts;
	name = deviceName;
	role = linkRole;

	if (! ports.complete())
		return false;

	// Sending needs no bind: a plain socket allowed to broadcast.
	socketFd = socket (AF_INET, SOCK_DGRAM, 0);
	if (socketFd < 0)
		return false;
	const int on = 1;
	setsockopt (socketFd, SOL_SOCKET, SO_BROADCAST, &on, sizeof (on));

	running = true;
	thread = std::thread ([this] { run(); });
	return true;
}

void ProLinkSender::stop()
{
	if (! running.exchange (false))
		return;
	if (thread.joinable())
		thread.join();
	close (socketFd);
	socketFd = -1;
}

void ProLinkSender::send (const std::vector<uint8_t>& packet, int port)
{
	sockaddr_in to {};
	to.sin_family = AF_INET;
	to.sin_port = htons ((uint16_t) port);
	to.sin_addr.s_addr = broadcastIp;
	sendto (socketFd, packet.data(), packet.size(), 0, reinterpret_cast<sockaddr*> (&to), sizeof (to));
}

void ProLinkSender::run()
{
	// One scheduler for whichever deck is master: its half-beat guard keeps a
	// handover from doubling a beat.
	BeatScheduler scheduler;
	double lastStatus = -1.0e9, lastPosition = -1.0e9;

	const auto idle = [&scheduler]
	{
		scheduler.step (now(), 0.0, 0.0, 0.0, 0.0, false);
		sleepFor (BeatScheduler::wakeEvery);
	};

	while (running)
	{
		// Nothing before the player number is ours.
		if (role != nullptr && ! role->isOnline())
		{
			idle();
			continue;
		}

		const auto t = now();
		const auto* beatDeck = master.load();
		const auto* deck = beatDeck != nullptr ? beatDeck : reported.load();
		const auto heard = deck != nullptr ? deckAt (*deck, t, pathLatency.load()) : DeckNow {};
		const auto state = playerState (heard, synced.load());
		if (role != nullptr)
			// Playing as the DJ set it: a scratch while playing must not hand
			// master away (the paused-master handoff).
			role->reportLocal (state.hasTrack, state.hasTrack && heard.playing, state.synced);

		if (t - lastStatus >= statusEvery)
		{
			// Without the link's role (tests): master while a MASTER deck has a tempo.
			const auto masterRole = role != nullptr ? role->masterState()
													: prolink::MasterState { beatDeck != nullptr && state.hasTrack };
			send (prolink::statusPacket (device(), name, state, masterRole), ports.status);
			lastStatus = t;
		}

		if (heard.loaded && t - lastPosition >= positionEvery)
		{
			send (prolink::absolutePosition (device(), name, positionOf (heard)), ports.beat);
			lastPosition = t;
		}

		// Beats only from a MASTER deck with a tempo.
		if (beatDeck == nullptr || ! heard.grid.isValid())
		{
			idle();
			continue;
		}

		const auto step = scheduler.step (t, heard.position, heard.rate, heard.grid.firstBeat, heard.grid.bpm, heard.moving);
		sleepFor (step.sleepBefore);
		if (step.send)
		{
			const auto msToEnd = heard.length > 0.0 ? (heard.length - step.send->trackSeconds) * 1000.0
													: std::numeric_limits<double>::infinity();
			send (prolink::beatPacket (device(), name, heard.grid.bpm, heard.speed, step.send->beatInBar, msToEnd),
				  ports.beat);
		}
	}
}
