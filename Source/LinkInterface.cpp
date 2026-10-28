// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "LinkInterface.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ifaddrs.h>
#include <limits>
#include <net/if.h>
#include <netinet/in.h>
#include <sstream>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace linkinterface
{
	uint32_t broadcastOf (uint32_t local, uint32_t netmask)
	{
		return local | ~netmask;
	}

	std::optional<std::string> defaultRouteInterface (const std::string& procNetRoute)
	{
		constexpr unsigned long routeUp = 0x1;   // RTF_UP

		std::optional<std::string> best;
		long bestMetric = std::numeric_limits<long>::max();
		std::istringstream lines (procNetRoute);
		std::string line;
		std::getline (lines, line);   // the header
		while (std::getline (lines, line))
		{
			std::istringstream fields (line);
			std::string name, destination, gateway, flags, refCount, use, metric, mask;
			if (! (fields >> name >> destination >> gateway >> flags >> refCount >> use >> metric >> mask))
				continue;
			if (destination != "00000000" || mask != "00000000")
				continue;
			char* end = nullptr;
			const auto flagBits = std::strtoul (flags.c_str(), &end, 16);
			if (*end != '\0' || (flagBits & routeUp) == 0)
				continue;
			const auto metricValue = std::strtol (metric.c_str(), &end, 10);
			if (*end != '\0' || metricValue >= bestMetric)
				continue;
			best = name;
			bestMetric = metricValue;
		}
		return best;
	}

	std::optional<Candidate> choose (const std::vector<Candidate>& found, const std::string& named,
									 const std::optional<std::string>& defaultRoute)
	{
		const auto byName = [&found] (const std::string& name) -> std::optional<Candidate>
		{
			for (const auto& candidate : found)
				if (candidate.name == name)
					return candidate;
			return std::nullopt;
		};

		if (! named.empty())
			return byName (named);
		if (defaultRoute)
			if (auto viaRoute = byName (*defaultRoute))
				return viaRoute;
		if (found.empty())
			return std::nullopt;
		return found.front();
	}

	bool usable (unsigned flags)
	{
		return ! (flags & IFF_LOOPBACK) && (flags & IFF_UP) && (flags & IFF_BROADCAST);
	}

	std::array<uint8_t, 6> hardwareAddress (const std::string& interfaceName)
	{
		std::array<uint8_t, 6> mac {};
		const auto probe = socket (AF_INET, SOCK_DGRAM, 0);
		if (probe < 0)
			return mac;
		ifreq request {};
		std::strncpy (request.ifr_name, interfaceName.c_str(), IFNAMSIZ - 1);
		if (ioctl (probe, SIOCGIFHWADDR, &request) == 0)
			std::memcpy (mac.data(), request.ifr_hwaddr.sa_data, 6);
		close (probe);
		return mac;
	}

	namespace
	{
		std::vector<Candidate> usableInterfaces()
		{
			std::vector<Candidate> found;
			ifaddrs* list = nullptr;
			if (getifaddrs (&list) < 0)
				return found;
			for (auto* ifa = list; ifa != nullptr; ifa = ifa->ifa_next)
			{
				if (ifa->ifa_addr == nullptr || ifa->ifa_netmask == nullptr || ifa->ifa_addr->sa_family != AF_INET)
					continue;
				if (! usable (ifa->ifa_flags))
					continue;
				found.push_back ({ ifa->ifa_name,
								   reinterpret_cast<sockaddr_in*> (ifa->ifa_addr)->sin_addr.s_addr,
								   reinterpret_cast<sockaddr_in*> (ifa->ifa_netmask)->sin_addr.s_addr });
			}
			freeifaddrs (list);
			return found;
		}

		std::string fileText (const char* path)
		{
			std::ifstream file (path);
			std::ostringstream text;
			text << file.rdbuf();
			return text.str();
		}
	}

	std::optional<Addresses> find (std::string& error)
	{
		const auto* namedValue = std::getenv (environmentVariable);
		const std::string named = namedValue != nullptr ? namedValue : "";

		const auto chosen = choose (usableInterfaces(), named, defaultRouteInterface (fileText ("/proc/net/route")));
		if (! chosen)
		{
			error = named.empty() ? "no network" : named + " is not up";
			return std::nullopt;
		}
		return Addresses { chosen->name, chosen->local, broadcastOf (chosen->local, chosen->netmask),
						   hardwareAddress (chosen->name) };
	}
}
