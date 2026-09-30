#include "ProLinkSender.h"

#include "BeatScheduler.h"
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
	constexpr double statusEvery = 0.2;

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

bool ProLinkSender::start (int number, uint32_t broadcastIpNetworkOrder,
						   const osctruth::ProLinkPorts& truthPorts, const std::string& deviceName)
{
	stop();
	deviceNumber = number;
	broadcastIp = broadcastIpNetworkOrder;
	ports = truthPorts;
	name = deviceName;

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
	double lastStatus = 0.0;

	while (running)
	{
		const auto* player = master.load();
		const auto grid = player != nullptr ? player->getBeatGrid() : BeatGrid {};

		// Nothing without a master deck with a tempo -- not even a status: a
		// "master" with no beats would take master from a device that has them.
		if (player == nullptr || ! grid.isValid())
		{
			scheduler.step (now(), 0.0, 0.0, 0.0, 0.0, false);
			sleepFor (BeatScheduler::wakeEvery);
			continue;
		}

		const auto moving = player->isPlaying() && ! player->isScratching();
		const auto rate = player->getEffectiveRate();
		const auto t = now();
		const auto position = positionAt (player->getPosition(), player->getPositionStamp(), t, rate, moving);

		if (t - lastStatus >= statusEvery)
		{
			// Where we are in the bar: the beat last passed.
			uint32_t beatNumber = 0;
			int beatInBar = 1;
			if (position >= grid.firstBeat)
			{
				const auto passed = (long long) std::floor ((position - grid.firstBeat) / grid.beatLength());
				beatNumber = (uint32_t) (passed + 1);
				beatInBar = 1 + (int) (passed % 4);
			}
			send (prolink::statusPacket (deviceNumber, name, grid.bpm, player->getSpeed(), true, moving,
										 beatNumber, beatInBar),
				  ports.status);
			lastStatus = t;
		}

		const auto step = scheduler.step (t, position, rate, grid.firstBeat, grid.bpm, moving);
		sleepFor (step.sleepBefore);
		if (step.send)
			send (prolink::beatPacket (deviceNumber, name, grid.bpm, player->getSpeed(), step.send->beatInBar),
				  ports.beat);
	}
}
