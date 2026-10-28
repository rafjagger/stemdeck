// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ProLinkReceiver.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Which network interface the Pro DJ Link goes out on. The link is broadcast
// on the control LAN; the first interface that is up can be the audio NIC
// instead (a3nuc2: enp5s0 before eno1).
namespace linkinterface
{
	struct Candidate
	{
		std::string name;
		std::string ipv4;   // dotted quad
	};

	// The first candidate whose address lies in `subnet` ("a.b.c.d/len"), or
	// nothing -- the caller then keeps its own detection.
	std::optional<Candidate> chooseCandidate (const std::vector<Candidate>& found, const std::string& subnet);
	std::optional<std::string> choose (const std::vector<Candidate>& found, const std::string& subnet);

	struct Addresses
	{
		uint32_t local = 0, broadcast = 0;   // network byte order
		std::array<uint8_t, 6> mac {};
	};

	// Up, can broadcast, not loopback: what the link may use (ifa_flags).
	bool usable (unsigned interfaceFlags);

	// Zeros when the interface has none.
	std::array<uint8_t, 6> hardwareAddress (const std::string& interfaceName);

	// The control LAN as the truth at `truthPath` names it (network.address); empty when it does not.
	std::string controlNetwork (const std::string& truthPath);

	// The up, broadcast-capable, non-loopback interface in `subnet`; nothing for an empty one.
	std::optional<Addresses> controlLan (const std::string& subnet);
}

// Pin the receiver (and so the sender's broadcast) to the control LAN; without
// one in the truth or up here, it keeps finding its own interface at start().
inline void pinLinkToControlLan (ProLinkReceiver& receiver, const std::string& truthPath)
{
	if (const auto lan = linkinterface::controlLan (linkinterface::controlNetwork (truthPath)))
		receiver.useInterface (lan->local, lan->broadcast, lan->mac);
}
