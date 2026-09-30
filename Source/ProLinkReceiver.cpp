#include "ProLinkReceiver.h"

#include <JuceHeader.h>

#include <arpa/inet.h>
#include <cstring>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
	constexpr int keepAliveMs = 1500;

	double now() { return juce::Time::getMillisecondCounterHiRes() / 1000.0; }
}

ProLinkReceiver::~ProLinkReceiver()
{
	stop();
}

std::string ProLinkReceiver::error() const
{
	std::lock_guard<std::mutex> guard (lock);
	return lastError;
}

bool ProLinkReceiver::detectInterface()
{
	// The first IPv4 interface that is up, can broadcast and is not loopback,
	// as beat-analyzer picks it.
	ifaddrs* list = nullptr;
	if (getifaddrs (&list) < 0)
		return false;

	std::string ifName;
	for (auto* ifa = list; ifa != nullptr; ifa = ifa->ifa_next)
	{
		if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET)
			continue;
		if ((ifa->ifa_flags & IFF_LOOPBACK) || ! (ifa->ifa_flags & IFF_UP) || ! (ifa->ifa_flags & IFF_BROADCAST))
			continue;

		localIp = reinterpret_cast<sockaddr_in*> (ifa->ifa_addr)->sin_addr.s_addr;
		if (ifa->ifa_broadaddr != nullptr)
			broadcastIp = reinterpret_cast<sockaddr_in*> (ifa->ifa_broadaddr)->sin_addr.s_addr;
		else
			broadcastIp = localIp | ~reinterpret_cast<sockaddr_in*> (ifa->ifa_netmask)->sin_addr.s_addr;
		ifName = ifa->ifa_name;
		break;
	}
	freeifaddrs (list);

	if (ifName.empty())
		return false;

	const auto probe = socket (AF_INET, SOCK_DGRAM, 0);
	if (probe >= 0)
	{
		ifreq request {};
		std::strncpy (request.ifr_name, ifName.c_str(), IFNAMSIZ - 1);
		if (ioctl (probe, SIOCGIFHWADDR, &request) == 0)
			std::memcpy (mac.data(), request.ifr_hwaddr.sa_data, 6);
		close (probe);
	}
	return true;
}

bool ProLinkReceiver::openSocket (int& fd, int port)
{
	fd = socket (AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		return false;

	const int on = 1;
	setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on));
	setsockopt (fd, SOL_SOCKET, SO_BROADCAST, &on, sizeof (on));

	sockaddr_in address {};
	address.sin_family = AF_INET;
	address.sin_port = htons ((uint16_t) port);
	address.sin_addr.s_addr = htonl (INADDR_ANY);
	return bind (fd, reinterpret_cast<sockaddr*> (&address), sizeof (address)) == 0;
}

bool ProLinkReceiver::start (int number, const osctruth::ProLinkPorts& truthPorts,
							 const std::string& deviceName)
{
	stop();
	deviceNumber = number;
	ports = truthPorts;
	name = deviceName;

	const auto fail = [this] (const std::string& why)
	{
		for (auto* fd : { &sockAnnounce, &sockBeat, &sockStatus })
			if (*fd >= 0) { close (*fd); *fd = -1; }
		std::lock_guard<std::mutex> guard (lock);
		lastError = why;
		return false;
	};

	if (! ports.complete())
		return fail ("no a3-osc.json");

	if (! detectInterface())
		return fail ("no network");

	if (! openSocket (sockAnnounce, ports.announce)
		|| ! openSocket (sockBeat, ports.beat)
		|| ! openSocket (sockStatus, ports.status))
		return fail ("ports busy");

	{
		std::lock_guard<std::mutex> guard (lock);
		lastError.clear();
		events.clear();
	}
	running = true;
	thread = std::thread ([this] { run(); });
	return true;
}

void ProLinkReceiver::stop()
{
	if (! running.exchange (false))
		return;

	for (auto* fd : { &sockAnnounce, &sockBeat, &sockStatus })
		if (*fd >= 0) { ::shutdown (*fd, SHUT_RDWR); close (*fd); *fd = -1; }

	if (thread.joinable())
		thread.join();
}

std::vector<ProLinkReceiver::Event> ProLinkReceiver::drain()
{
	std::lock_guard<std::mutex> guard (lock);
	std::vector<Event> out;
	out.swap (events);
	return out;
}

void ProLinkReceiver::sendKeepAlive()
{
	const auto packet = prolink::keepAlive (deviceNumber, name, mac, localIp);

	sockaddr_in to {};
	to.sin_family = AF_INET;
	to.sin_port = htons ((uint16_t) ports.announce);
	to.sin_addr.s_addr = broadcastIp;
	sendto (sockAnnounce, packet.data(), packet.size(), 0, reinterpret_cast<sockaddr*> (&to), sizeof (to));
}

void ProLinkReceiver::run()
{
	uint8_t buffer[600]; // CDJ-3000 status packets reach 0x200 bytes

	sendKeepAlive();
	auto lastKeepAlive = juce::Time::getMillisecondCounter();

	while (running)
	{
		pollfd fds[] = { { sockAnnounce, POLLIN, 0 }, { sockBeat, POLLIN, 0 }, { sockStatus, POLLIN, 0 } };
		const auto ready = poll (fds, 3, 50); // wakes for the keep-alive too

		if (juce::Time::getMillisecondCounter() - lastKeepAlive >= (juce::uint32) keepAliveMs)
		{
			sendKeepAlive();
			lastKeepAlive = juce::Time::getMillisecondCounter();
		}

		if (ready <= 0)
			continue;

		// Other devices' announcements: read and dropped, so the queue empties.
		if (fds[0].revents & POLLIN)
			recv (sockAnnounce, buffer, sizeof (buffer), 0);

		if (fds[1].revents & POLLIN)
		{
			const auto n = recv (sockBeat, buffer, sizeof (buffer), 0);
			const auto at = now();
			if (n > 0)
				if (auto beat = prolink::parseBeat (buffer, (size_t) n); beat && beat->device != deviceNumber)
				{
					std::lock_guard<std::mutex> guard (lock);
					events.push_back ({ *beat, at });
				}
		}

		if (fds[2].revents & POLLIN)
		{
			const auto n = recv (sockStatus, buffer, sizeof (buffer), 0);
			const auto at = now();
			if (n > 0)
				if (auto status = prolink::parseStatus (buffer, (size_t) n); status && status->device != 0 && status->device != deviceNumber)
				{
					std::lock_guard<std::mutex> guard (lock);
					events.push_back ({ *status, at });
				}
		}
	}
}
