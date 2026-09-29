#include "ProLinkSender.h"

#include "FollowLeader.h"
#include "StemDeckPlayer.h"

#include <JuceHeader.h>

#include <arpa/inet.h>
#include <cmath>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
	constexpr double wakeEvery = 0.005;      // seconds: picks up jumps, loops and tempo changes
	constexpr double statusEvery = 0.2;
	constexpr double sameBeatWindow = 0.1;   // a beat sent twice within this is one beat

	double now() { return juce::Time::getMillisecondCounterHiRes() / 1000.0; }

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

bool ProLinkSender::start (int number, uint32_t broadcastIpNetworkOrder, const std::string& deviceName)
{
	stop();
	deviceNumber = number;
	broadcastIp = broadcastIpNetworkOrder;
	name = deviceName;

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
	double lastStatus = 0.0;
	double lastBeatTrack = -1.0, lastBeatSent = -1.0;

	while (running)
	{
		const auto* player = master.load();
		if (player == nullptr)
		{
			sleepFor (wakeEvery);
			continue;
		}

		const auto grid = player->getBeatGrid();
		const auto moving = player->isPlaying() && ! player->isScratching();
		const auto rate = player->getEffectiveRate();
		const auto t = now();
		const auto position = positionAt (player->getPosition(), player->getPositionStamp(), t, rate, moving);

		// Where we are in the bar, for the status packet: the beat last passed.
		uint32_t beatNumber = 0;
		int beatInBar = 1;
		if (grid.isValid() && position >= grid.firstBeat)
		{
			const auto passed = (long long) std::floor ((position - grid.firstBeat) / grid.beatLength());
			beatNumber = (uint32_t) (passed + 1);
			beatInBar = 1 + (int) (passed % 4);
		}

		if (t - lastStatus >= statusEvery)
		{
			send (prolink::statusPacket (deviceNumber, name, grid.bpm, player->getSpeed(), true, moving,
										 beatNumber, beatInBar),
				  prolink::statusPort);
			lastStatus = t;
		}

		if (! moving || ! grid.isValid() || rate <= 0.0)
		{
			sleepFor (wakeEvery);
			continue;
		}

		const auto next = nextBeat (grid.firstBeat, grid.bpm, position);
		const auto wait = next ? (next->trackSeconds - position) / rate : wakeEvery;

		if (! next || wait > wakeEvery)
		{
			sleepFor (wakeEvery);
			continue;
		}

		// Due before the next wake-up: wait for it and send it, once. A loop
		// that brings the same beat back later is a new beat.
		sleepFor (wait);
		const auto sentAt = now();
		const auto sameBeat = std::abs (next->trackSeconds - lastBeatTrack) < 1e-6 && sentAt - lastBeatSent < sameBeatWindow;
		if (! sameBeat)
		{
			send (prolink::beatPacket (deviceNumber, name, grid.bpm, player->getSpeed(), next->beatInBar),
				  prolink::beatPort);
			lastBeatTrack = next->trackSeconds;
			lastBeatSent = sentAt;
		}
		sleepFor (0.001);
	}
}
