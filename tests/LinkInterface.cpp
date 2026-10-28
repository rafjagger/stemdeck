// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "LinkInterface.h"

#include <cstring>

// A CDJ has one network port and works on whatever network it is plugged
// into. StemDeck's host may have several (a3nuc2: control eno1, audio
// enp5s0), so it picks one by a rule -- never by a subnet written down here:
// the name in STEMDECK_PRODJLINK_INTERFACE if set, else the interface the
// default route leaves by, else the first that can broadcast. The broadcast
// address is the chosen interface's own address and netmask.
namespace
{
	uint32_t ip (int a, int b, int c, int d)
	{
		const uint8_t bytes[4] { (uint8_t) a, (uint8_t) b, (uint8_t) c, (uint8_t) d };
		uint32_t networkOrder = 0;
		std::memcpy (&networkOrder, bytes, 4);
		return networkOrder;
	}

	const linkinterface::Candidate audio { "enp5s0", ip (192, 168, 9, 20), ip (255, 255, 255, 0) };
	const linkinterface::Candidate control { "eno1", ip (192, 168, 8, 20), ip (255, 255, 255, 0) };
	const std::vector<linkinterface::Candidate> nuc2 { audio, control };   // getifaddrs' order there

	// /proc/net/route as Linux writes it: little-endian hex, flags 0003 = up + gateway.
	const std::string nuc2Routes =
		"Iface\tDestination\tGateway \tFlags\tRefCnt\tUse\tMetric\tMask\t\tMTU\tWindow\tIRTT\n"
		"enp5s0\t0009A8C0\t00000000\t0001\t0\t0\t0\t00FFFFFF\t0\t0\t0\n"
		"eno1\t00000000\t0108A8C0\t0003\t0\t0\t100\t00000000\t0\t0\t0\n"
		"eno1\t0008A8C0\t00000000\t0001\t0\t0\t100\t00FFFFFF\t0\t0\t0\n";
}

TEST (LinkInterface, TheBroadcastComesFromTheAddressAndItsNetmask)
{
	EXPECT_EQ (linkinterface::broadcastOf (ip (192, 168, 8, 20), ip (255, 255, 255, 0)), ip (192, 168, 8, 255));
	EXPECT_EQ (linkinterface::broadcastOf (ip (10, 1, 2, 3), ip (255, 0, 0, 0)), ip (10, 255, 255, 255));
	EXPECT_EQ (linkinterface::broadcastOf (ip (172, 16, 5, 4), ip (255, 255, 252, 0)), ip (172, 16, 7, 255));
	EXPECT_EQ (linkinterface::broadcastOf (ip (169, 254, 3, 7), ip (255, 255, 0, 0)), ip (169, 254, 255, 255))
		<< "a self-assigned address, as a CDJ takes without DHCP";
}

TEST (LinkInterface, TheDefaultRouteNamesTheInterface)
{
	EXPECT_EQ (linkinterface::defaultRouteInterface (nuc2Routes), std::optional<std::string> ("eno1"));
}

TEST (LinkInterface, OfTwoDefaultRoutesTheLowerMetricWins)
{
	const std::string routes =
		"Iface\tDestination\tGateway \tFlags\tRefCnt\tUse\tMetric\tMask\t\tMTU\tWindow\tIRTT\n"
		"wlan0\t00000000\t0100000A\t0003\t0\t0\t600\t00000000\t0\t0\t0\n"
		"eno1\t00000000\t0108A8C0\t0003\t0\t0\t100\t00000000\t0\t0\t0\n";
	EXPECT_EQ (linkinterface::defaultRouteInterface (routes), std::optional<std::string> ("eno1"));
}

TEST (LinkInterface, NoDefaultRouteIsNone)
{
	const std::string routes =
		"Iface\tDestination\tGateway \tFlags\tRefCnt\tUse\tMetric\tMask\t\tMTU\tWindow\tIRTT\n"
		"enp5s0\t0009A8C0\t00000000\t0001\t0\t0\t0\t00FFFFFF\t0\t0\t0\n"
		"eno1\t00000000\t0108A8C0\t0002\t0\t0\t100\t00000000\t0\t0\t0\n";   // a default route, but down
	EXPECT_EQ (linkinterface::defaultRouteInterface (routes), std::nullopt);
	EXPECT_EQ (linkinterface::defaultRouteInterface (""), std::nullopt);
	EXPECT_EQ (linkinterface::defaultRouteInterface ("garbage\n\t\t\n"), std::nullopt);
}

TEST (LinkInterface, TheDefaultRoutesInterfaceWinsOverTheFirst)
{
	const auto chosen = linkinterface::choose (nuc2, "", linkinterface::defaultRouteInterface (nuc2Routes));
	ASSERT_TRUE (chosen);
	EXPECT_EQ (chosen->name, "eno1");
	EXPECT_EQ (chosen->local, control.local);
}

TEST (LinkInterface, ANamedInterfaceWinsOverTheDefaultRoute)
{
	const auto chosen = linkinterface::choose (nuc2, "enp5s0", std::string ("eno1"));
	ASSERT_TRUE (chosen);
	EXPECT_EQ (chosen->name, "enp5s0");
}

// A name that is not up is a mistake to be seen, not a reason to pick another
// network quietly.
TEST (LinkInterface, ANamedInterfaceThatIsNotUpIsNone)
{
	EXPECT_EQ (linkinterface::choose (nuc2, "eno2", std::string ("eno1")), std::nullopt);
}

TEST (LinkInterface, WithoutADefaultRouteTheFirstIsTaken)
{
	const auto chosen = linkinterface::choose (nuc2, "", std::nullopt);
	ASSERT_TRUE (chosen);
	EXPECT_EQ (chosen->name, "enp5s0");

	const auto stale = linkinterface::choose (nuc2, "", std::string ("wlan0"));
	ASSERT_TRUE (stale) << "a default route on an interface without IPv4 up";
	EXPECT_EQ (stale->name, "enp5s0");

	EXPECT_EQ (linkinterface::choose ({}, "", std::string ("eno1")), std::nullopt);
}

// Two IPv4 addresses on the chosen interface: the first one, as getifaddrs lists it.
TEST (LinkInterface, AnAliasDoesNotChangeTheChoice)
{
	const std::vector<linkinterface::Candidate> aliased {
		{ "eno1", ip (192, 168, 8, 20), ip (255, 255, 255, 0) }, { "eno1", ip (10, 0, 0, 5), ip (255, 0, 0, 0) } };
	const auto chosen = linkinterface::choose (aliased, "", std::string ("eno1"));
	ASSERT_TRUE (chosen);
	EXPECT_EQ (chosen->local, ip (192, 168, 8, 20));
}

TEST (LinkInterface, UsableMeansUpBroadcastAndNotLoopback)
{
	EXPECT_TRUE (linkinterface::usable (0x1 | 0x2));        // IFF_UP | IFF_BROADCAST
	EXPECT_FALSE (linkinterface::usable (0x2));             // down
	EXPECT_FALSE (linkinterface::usable (0x1));             // no broadcast (a point-to-point link)
	EXPECT_FALSE (linkinterface::usable (0x1 | 0x2 | 0x8)); // loopback
}
