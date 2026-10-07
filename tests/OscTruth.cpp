#include <gtest/gtest.h>

#include "OscTruth.h"

#include <cstdlib>

// The Pro DJ Link ports come from the one truth, a3-core's a3-osc.json
// (decided 2026-09-30), like every port and address of the system. What the
// file says is read by a thin JUCE layer; what is taken from it is here.

namespace
{
	const std::vector<osctruth::Listener> listeners {
		{ "core", "osc", "any", 9000 },
		{ "prolink", "announce", "any", 60000 },
		{ "prolink", "beat", "any", 60001 },
		{ "prolink", "status", "any", 60002 },
	};
}

TEST (OscTruth, TheProLinkPortsAreTheTruths)
{
	const auto ports = osctruth::proLinkPortsFrom (listeners);
	EXPECT_EQ (ports.announce, 60000);
	EXPECT_EQ (ports.beat, 60001);
	EXPECT_EQ (ports.status, 60002);
	EXPECT_TRUE (ports.complete());
}

// Nothing is made up: a port the file does not have is none, and the PIO
// clock says so rather than opening a socket on a number from the code.
TEST (OscTruth, AMissingPortIsNone)
{
	const auto ports = osctruth::proLinkPortsFrom ({ listeners[0], listeners[1] });
	EXPECT_EQ (ports.announce, 60000);
	EXPECT_EQ (ports.beat, -1);
	EXPECT_EQ (ports.status, -1);
	EXPECT_FALSE (ports.complete());
}

TEST (OscTruth, NoFileIsNoPorts)
{
	EXPECT_FALSE (osctruth::proLinkPortsFrom ({}).complete());
}

TEST (OscTruth, TheEnvironmentPointsAtAnotherFile)
{
	setenv ("A3_OSC_TRUTH", "/tmp/elsewhere.json", 1);
	EXPECT_EQ (osctruth::truthPath(), "/tmp/elsewhere.json");
	unsetenv ("A3_OSC_TRUTH");
	EXPECT_EQ (osctruth::truthPath(), "/usr/share/a3/a3-osc.json");
}

// Where StemDeck sends and listens (spec stemdeck-remote): Core's and the
// desk's addresses by their host names, its own port. Core's listener says
// "any"; the address the others reach it at is the `core` host.
TEST (OscTruth, EndpointsComeFromListenersAndHosts)
{
	const std::vector<osctruth::Listener> l {
		{ "core", "osc", "any", 9000 }, { "mixer", "osc", "mixer", 7772 }, { "stemdeck", "osc", "any", 7780 } };
	const std::map<std::string, std::string> hosts { { "core", "192.168.8.10" }, { "mixer", "192.168.8.11" } };
	const auto e = osctruth::endpointsFrom (l, hosts);
	EXPECT_EQ (e.coreHost, "192.168.8.10");
	EXPECT_EQ (e.corePort, 9000);
	EXPECT_EQ (e.mixerHost, "192.168.8.11");
	EXPECT_EQ (e.mixerPort, 7772);
	EXPECT_EQ (e.ownPort, 7780);
	EXPECT_TRUE (e.complete());
}

TEST (OscTruth, AMissingListenerLeavesTheEndpointsIncomplete)
{
	const std::map<std::string, std::string> hosts { { "core", "192.168.8.10" } };
	EXPECT_FALSE (osctruth::endpointsFrom (listeners, hosts).complete());
}

// The meter ballistics are Core's, one set for every display (decided
// 2026-10-07): the truth's "meters" block, read into a map of its numbers by
// the JUCE layer. A truth without it -- or without one of them -- keeps the
// system's defaults.
TEST (OscTruth, TheMeterBallisticsAreTheTruths)
{
	const auto meters = osctruth::meterParametersFrom (
		{ { "attack_ms", 5.0 }, { "release_db_per_second", 30.0 }, { "peak_hold_seconds", 2.0 } });
	EXPECT_FLOAT_EQ (meters.attackMs, 5.0f);
	EXPECT_FLOAT_EQ (meters.releaseDbPerSecond, 30.0f);
	EXPECT_FLOAT_EQ (meters.peakHoldSeconds, 2.0f);
}

TEST (OscTruth, NoMetersBlockKeepsTheDefaults)
{
	const auto meters = osctruth::meterParametersFrom ({});
	EXPECT_FLOAT_EQ (meters.attackMs, 0.0f);
	EXPECT_FLOAT_EQ (meters.releaseDbPerSecond, 20.0f);
	EXPECT_FLOAT_EQ (meters.peakHoldSeconds, 1.5f);
}

TEST (OscTruth, AMissingMeterNumberKeepsItsDefault)
{
	const auto meters = osctruth::meterParametersFrom ({ { "release_db_per_second", 40.0 } });
	EXPECT_FLOAT_EQ (meters.attackMs, 0.0f);
	EXPECT_FLOAT_EQ (meters.releaseDbPerSecond, 40.0f);
	EXPECT_FLOAT_EQ (meters.peakHoldSeconds, 1.5f);
}

// A meter that never falls, or a negative time, is a typo, not a setting.
TEST (OscTruth, AnUnusableMeterNumberKeepsItsDefault)
{
	const auto meters = osctruth::meterParametersFrom (
		{ { "attack_ms", -1.0 }, { "release_db_per_second", 0.0 }, { "peak_hold_seconds", -0.5 } });
	EXPECT_FLOAT_EQ (meters.attackMs, 0.0f);
	EXPECT_FLOAT_EQ (meters.releaseDbPerSecond, 20.0f);
	EXPECT_FLOAT_EQ (meters.peakHoldSeconds, 1.5f);
}

TEST (OscTruth, AZeroHoldIsASetting)
{
	EXPECT_FLOAT_EQ (osctruth::meterParametersFrom ({ { "peak_hold_seconds", 0.0 } }).peakHoldSeconds, 0.0f);
}
