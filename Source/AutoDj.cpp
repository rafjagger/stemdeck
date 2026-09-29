#include "AutoDj.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr double pi = 3.14159265358979323846;

	double gainToDb (double gain)
	{
		return gain <= 0.001 ? AutoDj::silentDb : std::max (AutoDj::silentDb, 20.0 * std::log10 (gain));
	}

	double remaining (const AutoDj::DeckView& deck)
	{
		return (deck.length - deck.position) / std::max (0.01, deck.rate);
	}
}

double AutoDj::fadeInDb (double p)  { return gainToDb (std::sin (std::clamp (p, 0.0, 1.0) * pi / 2.0)); }
double AutoDj::fadeOutDb (double p) { return gainToDb (std::cos (std::clamp (p, 0.0, 1.0) * pi / 2.0)); }

void AutoDj::setEnabled (bool on)
{
	enabled = on;
	state = on ? Phase::starting : Phase::off;
	lastBarPosition = -1.0;
	progress = 0.0;
}

double AutoDj::mixLength (const DeckView& deck) const
{
	if (deck.gridBpm <= 0.0)
		return noGridMixSeconds * deck.rate;
	return mixBars * 4 * 60.0 / deck.gridBpm;   // track seconds: the grid's own beats
}

AutoDj::Commands AutoDj::update (const std::array<DeckView, 2>& decks)
{
	Commands out;
	if (! enabled)
		return out;

	const auto other = 1 - current;

	switch (state)
	{
		case Phase::off:
			break;

		case Phase::starting:
		{
			// Already playing: carry on from there. Else a track onto A.
			for (int d = 0; d < 2; ++d)
				if (decks[(size_t) d].playing)
				{
					current = d;
					state = Phase::playing;
					return out;
				}

			current = 0;
			out.load = 0;
			out.faderDb[0] = 0.0;
			state = Phase::ready;   // waits for the load, then starts it
			progress = -1.0;        // marks "first track": starts at once
			break;
		}

		case Phase::ready:
		{
			// The first track: as soon as it is there.
			if (progress < 0.0)
			{
				if (decks[(size_t) current].loaded)
				{
					out.start = current;
					state = Phase::playing;
					progress = 0.0;
				}
				break;
			}

			// The next one is loaded: wait for the mix point, then for a downbeat.
			const auto& playing = decks[(size_t) current];
			if (! playing.playing)
			{
				// Ended or stopped before the mix: straight on.
				out.start = other;
				out.faderDb[(size_t) other] = 0.0;
				current = other;
				state = Phase::playing;
				break;
			}

			const auto length = mixLength (playing);
			if (playing.looping || playing.length - playing.position > length)
				break;   // not yet -- or the DJ loops: the mix waits for the loop to end

			bool onDownbeat = playing.gridBpm <= 0.0;
			if (! onDownbeat)
			{
				const auto bar = 4 * 60.0 / playing.gridBpm;
				const auto inBar = std::fmod (std::max (0.0, playing.position - playing.firstBeat), bar);
				onDownbeat = lastBarPosition >= 0.0 && inBar < lastBarPosition;   // wrapped: a new bar began
				lastBarPosition = inBar;
			}
			if (! onDownbeat)
				break;

			out.start = other;
			if (playing.gridBpm > 0.0 && decks[(size_t) other].gridBpm > 0.0)
				out.syncOn = other;
			out.faderDb[(size_t) other] = silentDb;
			mixStart = playing.position;
			progress = 0.0;
			state = Phase::mixing;
			break;
		}

		case Phase::playing:
		{
			const auto& playing = decks[(size_t) current];
			if (! playing.loaded)
				break;
			if (decks[(size_t) other].looping)
				break;   // a loop there is the DJ's: not loaded over
			if (remaining (playing) <= mixLength (playing) / std::max (0.01, playing.rate) + loadAheadSeconds || ! playing.playing)
			{
				out.load = other;
				out.faderDb[(size_t) other] = silentDb;
				state = Phase::preparing;
			}
			break;
		}

		case Phase::preparing:
			if (decks[(size_t) other].loaded && ! decks[(size_t) other].playing)
			{
				state = Phase::ready;
				lastBarPosition = -1.0;
			}
			break;

		case Phase::mixing:
		{
			const auto& outgoing = decks[(size_t) current];
			progress = outgoing.playing ? (outgoing.position - mixStart) / mixLength (outgoing) : 1.0;

			if (progress < 1.0)
			{
				out.faderDb[(size_t) other] = fadeInDb (progress);
				out.faderDb[(size_t) current] = fadeOutDb (progress);
				break;
			}

			out.faderDb[(size_t) other] = 0.0;
			out.stop = current;
			out.syncOff = other;
			current = other;
			progress = 0.0;
			state = Phase::playing;
			break;
		}
	}

	return out;
}
