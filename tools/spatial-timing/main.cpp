// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
// The CPU gate's measurement without a test runner, to copy to a machine and run
// there: no JACK, no audio device, no network.
//   stemdeck-spatial-timing [blocks]      (default 2000)
#include "../../tests/spatial/SpatialTiming.h"

#include <cstdlib>
#include <iostream>

int main (int argc, char** argv)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	const int blocks = argc > 1 ? std::atoi (argv[1]) : 2000;
	if (blocks < 10)
	{
		std::cerr << "usage: stemdeck-spatial-timing [blocks >= 10]\n";
		return 2;
	}
	sdtest::printTable (std::cout, sdtest::spatialTiming (blocks));
	return 0;
}
