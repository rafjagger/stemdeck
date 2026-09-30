#pragma once

#include <string>
#include <vector>

// The one truth for every port and address of the A3 system: a3-core ships
// it as /usr/share/a3/a3-osc.json (decided 2026-09-30). StemDeck takes its
// Pro DJ Link ports from there. Pure, like ProLinkPackets: the file itself is
// read by OscTruthFile, which needs JUCE.
namespace osctruth
{
	// One entry of the file's `listeners`: who listens where.
	struct Listener
	{
		std::string program, role, host;
		int port = -1;
	};

	struct ProLinkPorts
	{
		int announce = -1, beat = -1, status = -1;

		bool complete() const { return announce > 0 && beat > 0 && status > 0; }
	};

	ProLinkPorts proLinkPortsFrom (const std::vector<Listener>& listeners);

	// $A3_OSC_TRUTH if set, else the installed file.
	std::string truthPath();
}
