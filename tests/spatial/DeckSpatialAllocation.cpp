// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "DeckSpatialFixture.h"

#include <atomic>
#include <cstdlib>
#include <new>

// Every form of the global operator new/delete is replaced for the whole test
// binary (as in a3-engine's SpatialAllocation.cpp), but only counts while the
// calling thread has armed it: other tests and other threads see the plain allocator.
namespace
{
thread_local bool countingArmed = false;
std::atomic<int> allocations { 0 };
std::atomic<int> deallocations { 0 };

void* allocate (std::size_t size, std::size_t alignment = 0)
{
	if (countingArmed)
		allocations.fetch_add (1, std::memory_order_relaxed);
	if (size == 0)
		size = 1;
	if (alignment <= alignof (std::max_align_t))
		return std::malloc (size);
	void* memory = nullptr;
	return posix_memalign (&memory, alignment, size) == 0 ? memory : nullptr;
}

void* allocateOrThrow (std::size_t size, std::size_t alignment = 0)
{
	if (auto* memory = allocate (size, alignment))
		return memory;
	throw std::bad_alloc();
}

void release (void* memory) noexcept
{
	if (memory == nullptr)
		return;
	if (countingArmed)
		deallocations.fetch_add (1, std::memory_order_relaxed);
	std::free (memory);
}

struct AllocationCount
{
	int allocated = 0, released = 0;
};

// Disarmed before the caller's assertions, whose failure messages allocate themselves.
template <typename Work>
AllocationCount countDuring (Work&& work)
{
	allocations = 0;
	deallocations = 0;
	countingArmed = true;
	work();
	countingArmed = false;
	return { allocations.load(), deallocations.load() };
}
}

void* operator new (std::size_t size) { return allocateOrThrow (size); }
void* operator new[] (std::size_t size) { return allocateOrThrow (size); }
void* operator new (std::size_t size, const std::nothrow_t&) noexcept { return allocate (size); }
void* operator new[] (std::size_t size, const std::nothrow_t&) noexcept { return allocate (size); }
void* operator new (std::size_t size, std::align_val_t alignment) { return allocateOrThrow (size, (std::size_t) alignment); }
void* operator new[] (std::size_t size, std::align_val_t alignment) { return allocateOrThrow (size, (std::size_t) alignment); }
void* operator new (std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept { return allocate (size, (std::size_t) alignment); }
void* operator new[] (std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept { return allocate (size, (std::size_t) alignment); }

void operator delete (void* memory) noexcept { release (memory); }
void operator delete[] (void* memory) noexcept { release (memory); }
void operator delete (void* memory, std::size_t) noexcept { release (memory); }
void operator delete[] (void* memory, std::size_t) noexcept { release (memory); }
void operator delete (void* memory, const std::nothrow_t&) noexcept { release (memory); }
void operator delete[] (void* memory, const std::nothrow_t&) noexcept { release (memory); }
void operator delete (void* memory, std::align_val_t) noexcept { release (memory); }
void operator delete[] (void* memory, std::align_val_t) noexcept { release (memory); }
void operator delete (void* memory, std::size_t, std::align_val_t) noexcept { release (memory); }
void operator delete[] (void* memory, std::size_t, std::align_val_t) noexcept { release (memory); }
void operator delete (void* memory, std::align_val_t, const std::nothrow_t&) noexcept { release (memory); }
void operator delete[] (void* memory, std::align_val_t, const std::nothrow_t&) noexcept { release (memory); }

using namespace sdtest;

// The counter itself: it must see an allocation, or a zero below proves nothing.
TEST (DeckSpatialAllocation, TheCounterSeesAnAllocation)
{
	const auto count = countDuring ([]
	{
		int* volatile single = new int (1);   // volatile: the optimiser may elide a new/delete pair
		delete single;
	});
	EXPECT_EQ (count.allocated, 1);
	EXPECT_EQ (count.released, 1);
}

TEST (DeckSpatialAllocation, ProcessDoesNotAllocate)
{
	juce::ScopedJuceInitialiser_GUI juceInitialiser;
	constexpr int blocks = 200;
	StemInputs in (blocks * block);
	for (int stem = 0; stem < DeckSpatial::stems; ++stem)
		in.playNoise (stem, stem + 1);
	Rendered out;
	for (auto& channel : out.bformat)
		channel = silence (blocks * block);
	for (auto* pair : { &out.cue, &out.aux, &out.phones })
		for (auto& channel : *pair)
			channel = silence (blocks * block);
	auto deck = preparedDeck (block);
	auto& c = deck->controls();

	const auto count = countDuring ([&]
	{
		for (int b = 0; b < blocks; ++b)
		{
			if (b % 10 == 0)
			{
				const int phase = b / 10;
				for (int s = 0; s < DeckSpatial::stems; ++s)
				{
					c.azimuth[(size_t) s] = -170.0f + 37.0f * (float) (phase + s);   // wraps past 180
					c.elevation[(size_t) s] = (float) ((phase * 7 + s * 11) % 60);
					c.width[(size_t) s] = 20.0f + 5.0f * (float) phase;
					c.threeD[(size_t) s] = (float) ((phase + s) % 3) * 0.5f;
					c.envelope[(size_t) s] = (phase % 4 == s) ? 1.0f : 0.0f;
					c.muted[(size_t) s] = (phase + s) % 2 == 0;
					c.cued[(size_t) s] = (phase + s) % 3 == 0;
				}
				c.deckThreeD = phase % 5 == 0 ? 0.0f : 1.0f;
				c.phonesDecoderOn = phase % 2 == 1;
				c.phonesMix = (float) (phase % 3) * 0.5f;
			}
			const int offset = b * block;
			std::array<const float*, 8> stems {};
			for (size_t ch = 0; ch < stems.size(); ++ch)
				stems[ch] = in.channels[ch].data() + offset;
			DeckSpatial::Outputs outputs;
			for (size_t ch = 0; ch < outputs.bformat.size(); ++ch)
				outputs.bformat[ch] = out.bformat[ch].data() + offset;
			for (size_t side = 0; side < 2; ++side)
			{
				outputs.cue[side] = out.cue[side].data() + offset;
				outputs.aux[side] = out.aux[side].data() + offset;
				outputs.phones[side] = out.phones[side].data() + offset;
			}
			// calls shorter and longer than the prepared block, too
			const int callSizes[] = { block, block, 1, 7, 300, 4096 };
			const int calls = juce::jmin (callSizes[b % 6], blocks * block - offset);
			deck->process (stems.data(), outputs, calls);
		}
	});
	EXPECT_EQ (count.allocated, 0);
	EXPECT_EQ (count.released, 0);
	EXPECT_GT (deck->decoderRuns().cue, 0) << "the cue decoder ran in the counted blocks";
	EXPECT_GT (deck->decoderRuns().phones, 0) << "the phones decoder ran in the counted blocks";
}
