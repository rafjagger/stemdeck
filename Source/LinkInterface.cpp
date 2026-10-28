// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "LinkInterface.h"
#include "OscTruthFile.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <cstring>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace linkinterface
{
	namespace
	{
		std::optional<uint32_t> hostOrder (const std::string& dottedQuad)
		{
			in_addr address {};
			if (inet_pton (AF_INET, dottedQuad.c_str(), &address) != 1)
				return std::nullopt;
			return ntohl (address.s_addr);
		}

		bool inSubnet (const std::string& ipv4, const std::string& subnet)
		{
			const auto slash = subnet.find ('/');
			if (slash == std::string::npos)
				return false;

			const auto network = hostOrder (subnet.substr (0, slash));
			const auto address = hostOrder (ipv4);
			if (! network || ! address)
				return false;

			const auto lengthText = subnet.substr (slash + 1);
			if (lengthText.empty() || lengthText.size() > 2 || lengthText.find_first_not_of ("0123456789") != std::string::npos)
				return false;
			const auto length = std::stoi (lengthText);
			if (length > 32)
				return false;

			const uint32_t mask = length == 0 ? 0u : ~uint32_t (0) << (32 - length);
			return (*network & mask) == (*address & mask);
		}
	}

	std::optional<Candidate> chooseCandidate (const std::vector<Candidate>& found, const std::string& subnet)
	{
		for (const auto& candidate : found)
			if (inSubnet (candidate.ipv4, subnet))
				return candidate;
		return std::nullopt;
	}

	std::optional<std::string> choose (const std::vector<Candidate>& found, const std::string& subnet)
	{
		if (const auto candidate = chooseCandidate (found, subnet))
			return candidate->name;
		return std::nullopt;
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

	std::string controlNetwork()
	{
		std::string error;
		return osctruth::readControlNetwork (osctruth::liveTruthPath(), error);
	}

	std::optional<Addresses> controlLan (const std::string& subnet)
	{
		if (subnet.empty())
			return std::nullopt;

		ifaddrs* list = nullptr;
		if (getifaddrs (&list) < 0)
			return std::nullopt;

		std::vector<Candidate> found;
		for (auto* ifa = list; ifa != nullptr; ifa = ifa->ifa_next)
		{
			if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET)
				continue;
			if (! usable (ifa->ifa_flags))
				continue;
			char text[INET_ADDRSTRLEN] {};
			inet_ntop (AF_INET, &reinterpret_cast<sockaddr_in*> (ifa->ifa_addr)->sin_addr, text, sizeof (text));
			found.push_back ({ ifa->ifa_name, text });
		}

		std::optional<Addresses> result;
		if (const auto chosen = chooseCandidate (found, subnet))
			for (auto* ifa = list; ifa != nullptr && ! result; ifa = ifa->ifa_next)
			{
				if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET || chosen->name != ifa->ifa_name)
					continue;
				const auto local = reinterpret_cast<sockaddr_in*> (ifa->ifa_addr)->sin_addr;
				char text[INET_ADDRSTRLEN] {};
				inet_ntop (AF_INET, &local, text, sizeof (text));
				if (chosen->ipv4 != text)
					continue;   // an alias of the same interface
				const auto netmask = reinterpret_cast<sockaddr_in*> (ifa->ifa_netmask)->sin_addr.s_addr;
				result = Addresses { local.s_addr, local.s_addr | ~netmask, hardwareAddress (chosen->name) };
			}

		freeifaddrs (list);
		return result;
	}
}
