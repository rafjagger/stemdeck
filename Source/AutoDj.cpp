#include "AutoDj.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
	using namespace StemHandover;

	double remaining (const AutoDj::DeckView& deck)
	{
		return (deck.length - deck.position) / std::max (0.01, deck.rate);
	}

	double barSeconds (const AutoDj::DeckView& deck)
	{
		return 4 * 60.0 / deck.gridBpm;   // track seconds: the grid's own beats
	}

	bool bothOnAGrid (const AutoDj::DeckView& a, const AutoDj::DeckView& b)
	{
		return a.gridBpm > 0.0 && b.gridBpm > 0.0;
	}
}

void AutoDj::setEnabled (bool on)
{
	enabled = on;
	state = on ? Phase::starting : Phase::off;
	lastBarPosition = -1.0;
	progress = 0.0;
	request.reset();
	mixNow = false;
	mix = {};
	// Mutes left from a mix it was turned off in are the DJ's now.
	ownMutes = {};
	releaseDeck = -1;
}

void AutoDj::setMixLength (MixLength length)
{
	setting.bars = length.bars <= 0 ? aboutTwentySeconds : std::max (minOverlapBars, length.bars);
	setting.noGridSeconds = length.noGridSeconds > 0.0 ? length.noGridSeconds : defaultNoGridMixSeconds;
}

int AutoDj::overlapBars (const DeckView& deck, MixLength length)
{
	if (length.bars != aboutTwentySeconds)
		return length.bars;
	const auto barHeard = barSeconds (deck) / std::max (0.01, deck.rate);
	return std::max (minOverlapBars, (int) std::lround (overlapSeconds / barHeard));
}

double AutoDj::mixSeconds (const DeckView& deck, bool synced, MixLength length)
{
	if (! synced || deck.gridBpm <= 0.0)
		return length.noGridSeconds * deck.rate;
	return overlapBars (deck, length) * barSeconds (deck);
}

void AutoDj::mute (Commands& out, const std::array<DeckView, 2>& decks, int deck, int stem)
{
	if (decks[(size_t) deck].muted[(size_t) stem])
		return;   // muted by the DJ: his, and so never unmuted here
	out.mute[(size_t) deck][(size_t) stem] = true;
	ownMutes[(size_t) deck][(size_t) stem] = true;
}

void AutoDj::unmute (Commands& out, int deck, int stem)
{
	if (! ownMutes[(size_t) deck][(size_t) stem])
		return;
	out.mute[(size_t) deck][(size_t) stem] = false;
	ownMutes[(size_t) deck][(size_t) stem] = false;
}

// Old out and new in on the same tick: never both, never neither.
void AutoDj::handOver (Commands& out, const std::array<DeckView, 2>& decks, int stem)
{
	mute (out, decks, current, stem);
	unmute (out, 1 - current, stem);
}

void AutoDj::forgetLiftedMutes (const std::array<DeckView, 2>& decks)
{
	for (size_t d = 0; d < 2; ++d)
		for (size_t s = 0; s < (size_t) numStems; ++s)
			if (ownMutes[d][s] && ! decks[d].muted[s])
				ownMutes[d][s] = false;
}

bool AutoDj::canMixNow() const
{
	if (! enabled || otherLoops)
		return false;
	return state == Phase::playing || state == Phase::preparing || (state == Phase::ready && progress >= 0.0);
}

bool AutoDj::requestMixNow (Pick target)
{
	if (! canMixNow())
		return false;
	request = target;
	return true;
}

// A Next / Prev pressed since the last tick: that track onto the other deck,
// over whatever waited there, and the mix at the next downbeat.
bool AutoDj::takeRequest (Commands& out)
{
	const auto target = request;
	request.reset();
	if (! target || ! canMixNow())
		return false;

	const auto other = 1 - current;
	out.load = other;
	out.loadPick = *target;
	out.faderDb[(size_t) other] = 0.0;
	state = Phase::preparing;
	mixNow = true;
	return true;
}

AutoDj::Commands AutoDj::update (const std::array<DeckView, 2>& decks)
{
	Commands out;
	if (! enabled)
		return out;

	forgetLiftedMutes (decks);
	// A tick after the stop, so the audio thread cannot play one more block
	// of the old track whole.
	if (releaseDeck >= 0)
	{
		for (int s = 0; s < numStems; ++s)
			unmute (out, releaseDeck, s);
		releaseDeck = -1;
	}

	const auto other = 1 - current;
	otherLoops = decks[(size_t) other].looping;
	if (takeRequest (out))
		return out;

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

			if (playing.looping)
				break;   // the DJ loops: the mix waits for the loop to end
			const auto synced = bothOnAGrid (playing, decks[(size_t) other]);
			if (! mixNow && playing.length - playing.position > mixSeconds (playing, synced, setting))
				break;   // not yet

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

			startMix (out, decks);
			break;
		}

		case Phase::playing:
		{
			const auto& playing = decks[(size_t) current];
			if (! playing.loaded)
				break;
			if (decks[(size_t) other].looping)
				break;   // a loop there is the DJ's: not loaded over
			const auto mixTime = mixSeconds (playing, playing.gridBpm > 0.0, setting) / std::max (0.01, playing.rate);
			if (remaining (playing) <= mixTime + loadAheadSeconds || ! playing.playing)
			{
				out.load = other;
				out.faderDb[(size_t) other] = 0.0;   // unity before it plays; never moved after
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
			if (mix.synced)
				runSyncedMix (out, decks);
			else
				runUnsyncedMix (out, decks);
			break;
	}

	return out;
}

// On the playing track's downbeat: the new track in with only its drums.
void AutoDj::startMix (Commands& out, const std::array<DeckView, 2>& decks)
{
	const auto other = 1 - current;
	const auto& outgoing = decks[(size_t) current];
	const auto& incoming = decks[(size_t) other];

	running = setting;
	mix = {};
	mix.synced = bothOnAGrid (outgoing, incoming);
	mix.toTrackEnd = ! mixNow;

	for (auto stem : { bass, StemHandover::other, vocals })
		mute (out, decks, other, stem);
	out.start = other;
	progress = 0.0;
	state = Phase::mixing;

	if (! mix.synced)
	{
		mix.start = outgoing.position;
		mix.length = mixSeconds (outgoing, false, running);
		return;
	}

	out.syncOn = other;
	mute (out, decks, current, drums);

	// The downbeat this tick found (it may be a tick past it), and the old
	// track's bars from there: the vocals change over on the first downbeat
	// at or after its end.
	const auto oldBar = barSeconds (outgoing);
	const auto downbeat = outgoing.position - std::fmod (std::max (0.0, outgoing.position - outgoing.firstBeat), oldBar);
	mix.endBar = mixNow ? overlapBars (outgoing, running)
						: std::max (1, (int) std::ceil ((outgoing.length - downbeat) / oldBar - 1.0e-6));

	std::vector<Downbeat> downbeats;
	for (int bar = 0; bar <= mix.endBar; ++bar)
		downbeats.push_back ({ bar, downbeat + bar * oldBar, incoming.firstBeat + bar * barSeconds (incoming) });

	static const Envelopes unheard {};
	const auto pair = chooseBassAndOther (outgoing.levels != nullptr ? *outgoing.levels : unheard,
										  incoming.levels != nullptr ? *incoming.levels : unheard,
										  downbeats, mix.endBar, window);
	mix.bassBar = pair.bass;
	mix.otherBar = pair.other;
	lastIncomingPosition = incoming.firstBeat;
}

// Counted on the new track's downbeats: synced, they are the old track's too,
// and they go on after the old track has ended.
void AutoDj::runSyncedMix (Commands& out, const std::array<DeckView, 2>& decks)
{
	const auto& outgoing = decks[(size_t) current];
	const auto& incoming = decks[(size_t) (1 - current)];
	const auto bar = barSeconds (incoming);
	const auto bars = (incoming.position - incoming.firstBeat) / bar;

	// A downbeat counts from half a tick before it: a mute due on it is then
	// at most half a tick early or late, rather than up to a whole tick late.
	const auto step = std::clamp (incoming.position - lastIncomingPosition, 0.0, 0.1);
	lastIncomingPosition = incoming.position;
	const auto at = (int) std::floor (bars + step / 2.0 / bar);

	const auto oldEnded = ! outgoing.playing || outgoing.position >= outgoing.length;
	if (oldEnded)
		mix.endBar = std::min (mix.endBar, (int) std::ceil (bars - 1.0e-6));   // stopped early: no waiting
	else if (mix.toTrackEnd && at >= mix.endBar)
		mix.endBar = at + 1;   // still playing (a loop, its tempo moved): the vocals wait for its end

	progress = std::clamp (bars / std::max (1, mix.endBar), 0.0, 1.0);

	if (at >= mix.endBar)
	{
		finishMix (out);
		return;
	}
	if (! mix.bassDone && at >= mix.bassBar)
	{
		handOver (out, decks, bass);
		mix.bassDone = true;
	}
	if (! mix.otherDone && at >= mix.otherBar)
	{
		handOver (out, decks, StemHandover::other);
		mix.otherDone = true;
	}
}

// Four steps over the mix time: drums in at the start; a third in, the bass;
// two thirds, old drums out and new other in; at the end the vocals.
void AutoDj::runUnsyncedMix (Commands& out, const std::array<DeckView, 2>& decks)
{
	const auto& outgoing = decks[(size_t) current];
	const auto other = 1 - current;
	progress = outgoing.playing ? std::clamp ((outgoing.position - mix.start) / mix.length, 0.0, 1.0) : 1.0;

	if (progress >= 1.0)
	{
		finishMix (out);
		return;
	}
	if (! mix.bassDone && progress >= 1.0 / 3.0)
	{
		handOver (out, decks, bass);
		mix.bassDone = true;
	}
	if (! mix.otherDone && progress >= 2.0 / 3.0)
	{
		mute (out, decks, current, drums);
		unmute (out, other, StemHandover::other);
		mix.otherDone = true;
	}
}

// The new track whole (what the DJ muted aside), the old deck stopped.
void AutoDj::finishMix (Commands& out)
{
	const auto other = 1 - current;
	for (int s = 0; s < numStems; ++s)
		unmute (out, other, s);
	out.stop = current;
	out.syncOff = other;
	releaseDeck = current;
	current = other;
	progress = 0.0;
	mixNow = false;
	mix = {};
	state = Phase::playing;
}
