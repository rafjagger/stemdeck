// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

// A deck's play mode, a CDJ-3000's Play Mode with REPEAT beside it: at the
// end of a set the deck stops (single), starts it again (repeat) or goes on
// with the next set of the library list the DJ sees (continuous: CONTINUE).
// One mode, so REPEAT and CONTINUE are never both on. Pure: no JUCE, testable.
//
// CONTINUE stages the next set -- its stems decoded into memory beside the
// playing one -- stageLeadSeconds before the end, and the player hands over
// to it at the end frame. It does nothing while the deck loops (a loop
// holds) or while Auto DJ runs (it owns the decks). At the last set of the
// list the deck stops, as a CDJ does. The next set starts at its start: like
// a load by hand, StemDeck has no Auto Cue.
namespace playmode
{
	enum class Mode { single, repeat, continuous };

	Mode toggledRepeat (Mode);
	Mode toggledContinue (Mode);
	// From a session's two flags; REPEAT wins if both are set.
	Mode fromFlags (bool repeat, bool continuePlay);

	// The set below `current` in `shown`; none at the end or for a set not shown.
	std::optional<std::size_t> nextIn (const std::vector<std::string>& shown, const std::string& current);

	// Long enough to decode a set of four FLAC stems (~0.3 s) many times over.
	constexpr double stageLeadSeconds = 10.0;

	struct Deck
	{
		Mode mode = Mode::single;
		bool looping = false, autoDj = false, playing = false;
		// Stopped at its end in CONTINUE with nothing to hand over to.
		bool ranOut = false;
		double position = 0.0, length = 0.0;   // seconds
		std::string current;                    // empty: nothing loaded
		std::string staged;                     // empty: nothing staged
	};

	// Whether the deck goes on to the next set at its end.
	bool continues (const Deck&);

	struct Plan
	{
		enum class Action { none, stage, unstage, loadAndPlay };
		Action action = Action::none;
		std::string id;   // stage, loadAndPlay
	};

	// What the deck's owner does now, called on its timer.
	Plan plan (const Deck&, const std::vector<std::string>& shown);
}
