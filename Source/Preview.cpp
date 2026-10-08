#include "Preview.h"

#include <algorithm>
#include <cmath>

namespace preview
{
	Ahead aheadAt (const std::vector<sections::Bar>& bars, double barPosition, std::optional<LoopBars> loop)
	{
		const auto count = (long) bars.size();
		const auto bar = std::max (0L, (long) std::floor (barPosition));

		if (bar >= count)
			return {};

		const auto& here = bars[(size_t) bar];
		Ahead ahead;
		ahead.section = sections::nameOf (here.section);
		ahead.energy = here.energy;

		auto change = bar + 1;
		while (change < count && bars[(size_t) change].section == here.section)
			++change;

		// A loop that turns back before the change keeps it from coming. As in
		// the player: a playhead short of the loop's end plays into it (also
		// from before its start); one past its end plays on freely.
		if (loop && barPosition < loop->end && loop->end <= (double) change)
		{
			ahead.next = ahead.section;
			ahead.barsUntilNext = -1;
			return ahead;
		}

		ahead.next = change < count ? sections::nameOf (bars[(size_t) change].section) : endWord;
		ahead.barsUntilNext = (int) (change - bar);
		return ahead;
	}

	int audibleDeck (const std::array<DeckView, 2>& decks, int masterDeck)
	{
		const auto audible = [&] (int d) { return decks[(size_t) d].playing && decks[(size_t) d].gain >= audibleFromGain; };

		if (audible (0) && audible (1))
		{
			if (masterDeck == 0 || masterDeck == 1)
				return masterDeck;
			return decks[1].gain > decks[0].gain ? 1 : 0;
		}
		if (audible (0))
			return 0;
		if (audible (1))
			return 1;
		return -1;
	}

	Moment momentOf (const std::array<DeckState, 2>& decks, int masterDeck)
	{
		Moment moment;
		moment.deck = audibleDeck ({ decks[0].view, decks[1].view }, masterDeck);

		if (moment.deck < 0)
			return moment;

		const auto& deck = decks[(size_t) moment.deck];
		moment.generation = deck.generation;

		if (deck.bpm <= 0.0)
			return moment;

		const auto barSeconds = 60.0 / deck.bpm * sections::beatsPerBar;
		if (deck.speed > 0.0)
			moment.barSeconds = barSeconds / deck.speed;
		const auto barsAt = [&] (double seconds) { return (seconds - deck.firstBeat) / barSeconds; };
		const auto barPosition = barsAt (deck.position);
		moment.bar = (int) std::floor (barPosition);

		std::optional<LoopBars> loop;
		if (deck.loopSeconds)
			loop = LoopBars { barsAt (deck.loopSeconds->first), barsAt (deck.loopSeconds->second) };

		moment.ahead = aheadAt (deck.bars, barPosition, loop);
		return moment;
	}

	bool Gate::shouldSend (const Moment& now, double nowSeconds)
	{
		const auto aBarHasPassed = now.barSeconds > 0.0 && nowSeconds - lastSentAt >= now.barSeconds;
		if (last && *last == now && ! aBarHasPassed)
			return false;
		last = now;
		lastSentAt = nowSeconds;
		return true;
	}
}
