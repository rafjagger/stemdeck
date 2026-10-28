// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Which network interface the Pro DJ Link goes out on. A CDJ has one port and
// works on whatever network it is plugged into; a host can have several
// (a3nuc2: control eno1, audio enp5s0, the audio NIC listed first). The rule:
// the interface named in STEMDECK_PRODJLINK_INTERFACE if set; else the one the
// default route leaves by -- the network the machine is plugged into, where a
// closed audio network has no gateway; else the first that is up and can
// broadcast. The subnet and broadcast address come from the chosen
// interface's own address and netmask, never from a constant.
namespace linkinterface
{
	inline constexpr const char* environmentVariable = "STEMDECK_PRODJLINK_INTERFACE";

	// An interface's IPv4 address, network byte order.
	struct Candidate
	{
		std::string name;
		uint32_t local = 0, netmask = 0;
	};

	struct Addresses
	{
		std::string name;
		uint32_t local = 0, broadcast = 0;   // network byte order
		std::array<uint8_t, 6> mac {};
	};

	uint32_t broadcastOf (uint32_t local, uint32_t netmask);

	// The interface of the up default route with the lowest metric in
	// /proc/net/route's text; nothing without one.
	std::optional<std::string> defaultRouteInterface (const std::string& procNetRoute);

	// The rule above over the usable candidates, in getifaddrs' order. A named
	// interface that is not among them is nothing: a mistake to be seen, not
	// a reason to go out on another network.
	std::optional<Candidate> choose (const std::vector<Candidate>& found, const std::string& named,
									 const std::optional<std::string>& defaultRoute);

	// Up, can broadcast, not loopback: what the link may use (ifa_flags).
	bool usable (unsigned interfaceFlags);

	// Zeros when the interface has none.
	std::array<uint8_t, 6> hardwareAddress (const std::string& interfaceName);

	// The rule applied to this machine; nothing, with `error` set, when no
	// interface qualifies.
	std::optional<Addresses> find (std::string& error);
}
