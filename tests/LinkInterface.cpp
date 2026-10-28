// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "LinkInterface.h"
#include "OscTruthFile.h"

#include <filesystem>
#include <fstream>

// Pro DJ Link must go out on the control LAN (broadcast on eno1), not on
// whichever interface is up first -- on a3nuc2 that can be the audio NIC.
TEST (LinkInterface, ControlLanWinsOverTheFirstInterface)
{
	const std::vector<linkinterface::Candidate> found {
		{ "enp5s0", "192.168.9.20" }, { "eno1", "192.168.8.20" } };
	EXPECT_EQ (linkinterface::choose (found, "192.168.8.0/24"), std::optional<std::string> ("eno1"));
}

TEST (LinkInterface, NoneOnTheControlLanIsNone)
{
	const std::vector<linkinterface::Candidate> found { { "enp5s0", "192.168.9.20" }, { "wlan0", "10.0.0.4" } };
	EXPECT_EQ (linkinterface::choose (found, "192.168.8.0/24"), std::nullopt) << "the caller falls back to its detection";
	EXPECT_EQ (linkinterface::choose ({}, "192.168.8.0/24"), std::nullopt);
}

// Two IPv4 addresses on one interface: the one on the control LAN is the one
// the link must announce with.
TEST (LinkInterface, TheAddressInTheSubnetWinsNotTheInterfaceName)
{
	const std::vector<linkinterface::Candidate> found {
		{ "eno1", "10.0.0.5" }, { "eno1", "192.168.8.20" } };
	const auto chosen = linkinterface::chooseCandidate (found, "192.168.8.0/24");
	ASSERT_TRUE (chosen);
	EXPECT_EQ (chosen->ipv4, "192.168.8.20");
}

TEST (LinkInterface, SubnetEdgesAndPrefixLengths)
{
	EXPECT_TRUE (linkinterface::choose ({ { "a", "192.168.8.255" } }, "192.168.8.0/24"));
	EXPECT_FALSE (linkinterface::choose ({ { "a", "192.168.7.255" } }, "192.168.8.0/24"));
	EXPECT_FALSE (linkinterface::choose ({ { "a", "192.168.9.1" } }, "192.168.8.0/24"));
	EXPECT_TRUE (linkinterface::choose ({ { "a", "10.1.2.3" } }, "10.0.0.0/8"));
	EXPECT_FALSE (linkinterface::choose ({ { "a", "192.168.8.1" } }, "garbage")) << "a bad subnet matches nothing";
	EXPECT_FALSE (linkinterface::choose ({ { "a", "garbage" } }, "192.168.8.0/24"));
}

// The control LAN is the truth's (network.address, the Core's host address and
// its prefix), not a literal here.
TEST (LinkInterface, TheTruthsHostAddressNamesTheSubnet)
{
	const std::vector<linkinterface::Candidate> found {
		{ "enp5s0", "192.168.9.20" }, { "eno1", "192.168.8.20" } };
	EXPECT_EQ (linkinterface::choose (found, "192.168.8.10/24"), std::optional<std::string> ("eno1"));
}

// No network in the truth: nothing is pinned, and the receiver keeps finding
// its own interface at start().
TEST (LinkInterface, NoControlNetworkIsNoControlLan)
{
	EXPECT_FALSE (linkinterface::controlLan (""));
}

namespace
{
	std::string truthFile (const std::string& name, const std::string& text)
	{
		const auto path = std::filesystem::temp_directory_path() / name;
		std::ofstream (path) << text;
		return path.string();
	}
}

TEST (LinkInterface, TheControlNetworkIsReadFromTheTruth)
{
	std::string error;
	const auto with = truthFile ("stemdeck-link-truth.json", R"({ "network": { "address": "192.168.8.10/24" } })");
	EXPECT_EQ (osctruth::readControlNetwork (with, error), "192.168.8.10/24");
	EXPECT_TRUE (error.empty());

	const auto without = truthFile ("stemdeck-link-truth-old.json", R"({ "hosts": {} })");
	EXPECT_EQ (osctruth::readControlNetwork (without, error), "") << "an older truth: today's detection";

	EXPECT_EQ (osctruth::readControlNetwork ("/nonexistent/a3-osc.json", error), "");
	EXPECT_FALSE (error.empty());
	std::filesystem::remove (with);
	std::filesystem::remove (without);
}
