// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "SpawnOutput.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <vector>

TEST (SpawnOutput, HandsBackWhatTheProgramPrinted)
{
	EXPECT_EQ (spawnForOutput ({ "printf", "%s", "2:STEMDECK" }, 1000), "2:STEMDECK");
}

TEST (SpawnOutput, AProgramThatIsNotThereIsAnEmptyAnswer)
{
	EXPECT_EQ (spawnForOutput ({ "no-such-program-a3" }, 1000), "");
}

TEST (SpawnOutput, AHangingProgramIsGivenUpAfterTheTimeout)
{
	const auto start = std::chrono::steady_clock::now();
	EXPECT_EQ (spawnForOutput ({ "sleep", "5" }, 200), "");
	EXPECT_LT (std::chrono::steady_clock::now() - start, std::chrono::seconds (2));
}

namespace
{
double medianSpawnMs()
{
	std::vector<double> times;
	for (int i = 0; i < 7; ++i)
	{
		const auto start = std::chrono::steady_clock::now();
		spawnForOutput ({ "true" }, 1000);
		times.push_back (std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - start).count());
	}
	std::sort (times.begin(), times.end());
	return times[times.size() / 2];
}
}

// The deck polls i3 every 500 ms while holding its decoded stems, ~700 MB.
// fork() copies the page tables of all of it, ~10 ms with the address space
// locked, and the audio thread stalls behind that: xruns twice a second
// (a3nuc2, 2026-10-29). A spawn must cost the same however much the deck holds.
TEST (SpawnOutput, CostsNoMoreWhenTheCallerHoldsMuchMemory)
{
	const auto small = medianSpawnMs();

	std::vector<char> held (512u << 20);
	std::memset (held.data(), 1, held.size());
	const auto large = medianSpawnMs();

	EXPECT_LT (large, small * 3.0 + 2.0) << "small " << small << " ms, large " << large << " ms";
	EXPECT_EQ (held[held.size() / 2], 1);
}
