#include "AutoDj.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
	using namespace StemHandover;

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
	levelsWaitFrom = -1.0;
	routing = false;
	djStems = {};
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

double AutoDj::audibleEnd (const DeckView& deck)
{
	return deck.audibleEnd ? std::min (*deck.audibleEnd, deck.length) : deck.length;
}

// The first downbeat of the track's sound (see the class comment); a pickup
// before the grid's first downbeat cannot be played on the beat and is left out.
double AutoDj::startPosition (const DeckView& deck)
{
	if (deck.gridBpm <= 0.0)
		return deck.audibleStart.value_or (0.0);
	if (! deck.audibleStart || *deck.audibleStart <= deck.firstBeat)
		return deck.firstBeat;

	const auto bar = barSeconds (deck), beat = bar / 4.0;
	const auto heard = *deck.audibleStart;
	const auto opening = deck.firstBeat + std::floor ((heard - deck.firstBeat) / bar) * bar;
	const auto into = heard - opening;
	return into <= onTheDownbeat || into >= bar - beat ? opening : opening + bar;
}

double AutoDj::mixSeconds (const DeckView& deck, bool synced, MixLength length)
{
	if (! synced || deck.gridBpm <= 0.0)
		return length.noGridSeconds * deck.rate;
	return overlapBars (deck, length) * barSeconds (deck);
}

void AutoDj::takeOver (int deck)
{
	routing = true;
	source.fill (deck);
}

// Bus `stem` goes over to the new deck. A stem of the old deck the DJ put on
// that bus goes back to AutoDJ, which then places it as it places the rest.
void AutoDj::handOver (const std::array<DeckView, 2>& decks, int stem)
{
	const auto old = source[(size_t) stem];
	for (int s = 0; s < numStems; ++s)
		if (decks[(size_t) old].bus[(size_t) s] == stem)
			djStems[(size_t) old][(size_t) s] = false;
	source[(size_t) stem] = 1 - current;
	mix.handedOver[(size_t) stem] = true;
}

// Every stem not the DJ's where it belongs: stem n of the deck bus n is with
// on bus n, unless the DJ put a stem there; every other stem on no bus. All
// in one tick, so a bus switches source without a gap or two stems on it.
void AutoDj::routeEveryStem (Commands& out, const std::array<DeckView, 2>& decks) const
{
	std::array<bool, numStems> heldByDj {};
	for (size_t d = 0; d < 2; ++d)
		for (size_t s = 0; s < (size_t) numStems; ++s)
			if (djStems[d][s] && decks[d].bus[s] >= 0 && decks[d].bus[s] < numStems)
				heldByDj[(size_t) decks[d].bus[s]] = true;

	for (int d = 0; d < 2; ++d)
		for (int s = 0; s < numStems; ++s)
		{
			if (djStems[(size_t) d][(size_t) s])
				continue;
			auto wanted = source[(size_t) s] == d ? s : offBus;
			if (wanted == s && heldByDj[(size_t) s])
				wanted = offBus;
			if (decks[(size_t) d].bus[(size_t) s] != wanted)
				out.bus[(size_t) d][(size_t) s] = wanted;
		}
}

void AutoDj::djSwitched (int deck, int stem, int bus, bool on)
{
	if (deck < 0 || deck > 1 || stem < 0 || stem >= numStems)
		return;
	// AUX off: "not on the return" -- back to AutoDJ, which knows where it plays.
	djStems[(size_t) deck][(size_t) stem] = ! (bus == auxBus && ! on);
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
	levelsWaitFrom = -1.0;
	return true;
}

AutoDj::Commands AutoDj::update (const std::array<DeckView, 2>& decks)
{
	Commands out;
	if (! enabled)
		return out;
	step (out, decks);
	if (out.load >= 0)
		djStems[(size_t) out.load] = {};   // a new track: every stem AutoDJ's
	if (routing)
		routeEveryStem (out, decks);
	return out;
}

void AutoDj::step (Commands& out, const std::array<DeckView, 2>& decks)
{

	const auto other = 1 - current;
	otherLoops = decks[(size_t) other].looping;
	if (takeRequest (out))
		return;

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
					takeOver (d);
					state = Phase::playing;
					return;
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
					takeOver (current);
					out.start = current;
					out.startAt = startPosition (decks[(size_t) current]);
					state = Phase::playing;
					progress = 0.0;
				}
				break;
			}

			// The next one is loaded: wait for the mix point, then for a downbeat.
			const auto& playing = decks[(size_t) current];
			if (! playing.playing || playing.position >= audibleEnd (playing))
			{
				// Ended, or stopped, before the mix: straight on.
				takeOver (other);
				out.stop = current;
				out.start = other;
				out.startAt = startPosition (decks[(size_t) other]);
				out.faderDb[(size_t) other] = 0.0;
				current = other;
				state = Phase::playing;
				break;
			}

			if (playing.looping)
			{
				lastBarPosition = -1.0;
				break;   // the DJ loops: the mix waits for the loop to end
			}
			const auto synced = bothOnAGrid (playing, decks[(size_t) other]);
			if (! mixNow && audibleEnd (playing) - playing.position > mixSeconds (playing, synced, setting))
				break;   // not yet

			// Where the new track is heard decides where it starts: worth a
			// few seconds' wait while its levels are read (a mix now, say).
			if (decks[(size_t) other].levelsPending)
			{
				if (levelsWaitFrom < 0.0)
					levelsWaitFrom = playing.position;
				if ((playing.position - levelsWaitFrom) / std::max (0.01, playing.rate) < maxLevelsWait)
				{
					lastBarPosition = -1.0;
					break;
				}
			}

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

			levelsWaitFrom = -1.0;
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
			const auto remaining = (audibleEnd (playing) - playing.position) / std::max (0.01, playing.rate);
			if (remaining <= mixTime + loadAheadSeconds || ! playing.playing)
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

}

// On the playing track's downbeat: the new track in with only its drums --
// without a grid too, since one bus carries one drum part: two unsynced ones
// at once would only trip over each other.
void AutoDj::startMix (Commands& out, const std::array<DeckView, 2>& decks)
{
	const auto other = 1 - current;
	const auto& outgoing = decks[(size_t) current];
	const auto& incoming = decks[(size_t) other];

	running = setting;
	mix = {};
	mix.synced = bothOnAGrid (outgoing, incoming);
	mix.toTrackEnd = ! mixNow;

	handOver (decks, drums);
	out.start = other;
	out.startAt = startPosition (incoming);
	mix.incomingStart = out.startAt;
	progress = 0.0;
	state = Phase::mixing;

	if (! mix.synced)
	{
		mix.start = outgoing.position;
		mix.length = mixSeconds (outgoing, false, running);
		return;
	}

	out.syncOn = other;

	// The downbeat this tick found (it may be a tick past it), and the old
	// track's bars from there: the vocals change over on the first downbeat
	// at or after its end.
	const auto oldBar = barSeconds (outgoing);
	const auto downbeat = outgoing.position - std::fmod (std::max (0.0, outgoing.position - outgoing.firstBeat), oldBar);
	mix.endBar = mixNow ? overlapBars (outgoing, running)
						: std::max (1, (int) std::ceil ((audibleEnd (outgoing) - downbeat) / oldBar - 1.0e-6));

	std::vector<Downbeat> downbeats;
	for (int bar = 0; bar <= mix.endBar; ++bar)
		downbeats.push_back ({ bar, downbeat + bar * oldBar, mix.incomingStart + bar * barSeconds (incoming) });

	static const Envelopes unheard {};
	const auto pair = chooseBassAndOther (outgoing.levels != nullptr ? *outgoing.levels : unheard,
										  incoming.levels != nullptr ? *incoming.levels : unheard,
										  downbeats, mix.endBar, window);
	mix.bassBar = pair.bass;
	mix.otherBar = pair.other;
	lastIncomingPosition = mix.incomingStart;
}

// Counted on the new track's downbeats: synced, they are the old track's too,
// and they go on after the old track has ended.
void AutoDj::runSyncedMix (Commands& out, const std::array<DeckView, 2>& decks)
{
	const auto& outgoing = decks[(size_t) current];
	const auto& incoming = decks[(size_t) (1 - current)];
	const auto bar = barSeconds (incoming);
	const auto bars = (incoming.position - mix.incomingStart) / bar;

	// A downbeat counts from half a tick before it: a switch due on it is then
	// at most half a tick early or late, rather than up to a whole tick late.
	const auto step = std::clamp (incoming.position - lastIncomingPosition, 0.0, 0.1);
	lastIncomingPosition = incoming.position;
	const auto at = (int) std::floor (bars + step / 2.0 / bar);

	const auto oldEnded = ! outgoing.playing || outgoing.position >= audibleEnd (outgoing);
	if (oldEnded)
		mix.endBar = std::min (mix.endBar, (int) std::ceil (bars - 1.0e-6));   // stopped early: no waiting
	else if (mix.toTrackEnd && at >= mix.endBar)
		mix.endBar = at + 1;   // still playing (a loop, its tempo moved): the vocals wait for its end

	progress = std::clamp (bars / std::max (1, mix.endBar), 0.0, 1.0);

	if (at >= mix.endBar)
	{
		finishMix (out, decks);
		return;
	}
	if (! mix.handedOver[bass] && at >= mix.bassBar)
		handOver (decks, bass);
	if (! mix.handedOver[StemHandover::other] && at >= mix.otherBar)
		handOver (decks, StemHandover::other);
}

// Four steps over the mix time: the drums at the start; a third in, the
// bass; two thirds in, the other; at the end the vocals.
void AutoDj::runUnsyncedMix (Commands& out, const std::array<DeckView, 2>& decks)
{
	const auto& outgoing = decks[(size_t) current];
	const auto heard = outgoing.playing && outgoing.position < audibleEnd (outgoing);
	progress = heard ? std::clamp ((outgoing.position - mix.start) / mix.length, 0.0, 1.0) : 1.0;

	if (progress >= 1.0)
	{
		finishMix (out, decks);
		return;
	}
	if (! mix.handedOver[bass] && progress >= 1.0 / 3.0)
		handOver (decks, bass);
	if (! mix.handedOver[StemHandover::other] && progress >= 2.0 / 3.0)
		handOver (decks, StemHandover::other);
}

// The vocals, and whatever has not changed over yet (an old track stopped
// early); the old deck stops on no bus.
void AutoDj::finishMix (Commands& out, const std::array<DeckView, 2>& decks)
{
	const auto other = 1 - current;
	for (int s = 0; s < numStems; ++s)
		if (! mix.handedOver[(size_t) s])
			handOver (decks, s);
	djStems[(size_t) current] = {};   // its stems AutoDJ's again, all off
	out.stop = current;
	out.syncOff = other;
	current = other;
	progress = 0.0;
	mixNow = false;
	mix = {};
	state = Phase::playing;
}
