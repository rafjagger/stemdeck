// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "PlayMode.h"

#include <algorithm>

namespace playmode
{
	Mode toggledRepeat (Mode mode)
	{
		return mode == Mode::repeat ? Mode::single : Mode::repeat;
	}

	Mode toggledContinue (Mode mode)
	{
		return mode == Mode::continuous ? Mode::single : Mode::continuous;
	}

	Mode fromFlags (bool repeat, bool continuePlay)
	{
		if (repeat)
			return Mode::repeat;
		return continuePlay ? Mode::continuous : Mode::single;
	}

	std::optional<std::size_t> nextIn (const std::vector<std::string>& shown, const std::string& current)
	{
		const auto at = std::find (shown.begin(), shown.end(), current);
		if (at == shown.end() || std::next (at) == shown.end())
			return std::nullopt;
		return (std::size_t) std::distance (shown.begin(), at) + 1;
	}

	bool continues (const Deck& deck)
	{
		return deck.mode == Mode::continuous && ! deck.looping && ! deck.autoDj && ! deck.current.empty();
	}

	Plan plan (const Deck& deck, const std::vector<std::string>& shown)
	{
		const auto unstage = [&deck]
		{
			return deck.staged.empty() ? Plan {} : Plan { Plan::Action::unstage, {} };
		};

		if (! continues (deck))
			return unstage();

		const auto next = nextIn (shown, deck.current);
		if (! next)
			return unstage();
		const auto& nextId = shown[*next];

		if (deck.ranOut)
			return { Plan::Action::loadAndPlay, nextId };

		if (deck.length - deck.position > stageLeadSeconds)
			return deck.staged == nextId ? Plan {} : unstage();
		return deck.staged == nextId ? Plan {} : Plan { Plan::Action::stage, nextId };
	}
}
