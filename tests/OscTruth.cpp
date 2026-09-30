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
