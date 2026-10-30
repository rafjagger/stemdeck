// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "DeckSpatial.h"
#include "MotionLite.h"

#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <utility>

// A deck's four one-shot movers (MotionLite.h) between the call-out and the
// audio thread. The call-out pushes presses and length keys into a lock-free
// single-producer queue; the audio thread drains it once per block, advances
// each stem in the deck's beat time and writes the played pose and 3D
// envelope. The home stays where the call-out and the session keep it (the
// settings); what the audio thread last did is published for the viewer.
class DeckMotion
{
public:
	static constexpr int stems = spatial::stemsPerDeck;

	DeckMotion();

	// Message thread, the one producer. False when the queue is full.
	bool press (int stem, motionlite::Shape, bool shift, std::uint64_t seed);
	bool lengthDown (int stem);
	bool lengthUp (int stem);
	bool setLength (int stem, double bars);   // a restored session; anything outside the valid set is 4

	// The consumer's side, before the first block (DeckSpatialPorts::prepare):
	// presses queued while the audio stood are dropped, length changes kept.
	// Never while advance() can run.
	void dropPendingPresses();

	// Audio thread, once per block: settings give each stem's home and width;
	// played gets the pose, the width and the envelope. Allocation-free.
	void advance (const motionlite::Clock&, double blockSeconds, const DeckSpatial::Controls& settings,
				  DeckSpatial::Controls& played);

	// Any thread. The length is what the message thread last asked for, so a
	// session restored before the first block reads back as it was restored.
	// Everything else here is as of the last advance.
	motionlite::PadState padState (int stem, motionlite::Shape) const;
	motionlite::PadState stemState (int stem) const;   // running, else armed, else idle
	double lengthBars (int stem) const;
	spatial::Pose posePlayed (int stem) const;
	std::optional<std::pair<int, int>> progress (int stem) const;   // beat now, beats in all
	std::optional<double> beat() const;                              // the deck's beat; none without a grid

private:
	enum class Kind : std::uint8_t { press, lengthDown, lengthUp, setLength };
	struct Command
	{
		Kind kind = Kind::press;
		int stem = 0;
		motionlite::Shape shape = motionlite::Shape::orbit;
		bool shift = false;
		std::uint64_t seed = 0;
		double bars = 0.0;
	};

	struct Published
	{
		std::array<std::atomic<std::uint8_t>, 4> pads {};
		std::atomic<float> azimuth { 0.0f }, elevation { 0.0f }, width { 60.0f };
		std::atomic<int> progressNow { 0 }, progressTotal { 0 };
	};

	static constexpr int queueSize = 64;

	bool push (const Command&);
	void drain (bool presses = true);
	void publish (int stem, const spatial::Pose& played);

	juce::AbstractFifo fifo { queueSize };
	std::array<Command, queueSize> commands {};
	std::array<motionlite::StemMotion, stems> motions {};
	std::array<Published, stems> published {};
	std::array<std::atomic<double>, stems> lengthMirror { 4.0, 4.0, 4.0, 4.0 };
	std::atomic<double> lastBeat { 0.0 };
	std::atomic<bool> hasBeat { false };
};
