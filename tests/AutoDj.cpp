#include <gtest/gtest.h>

#include "AutoDj.h"
#include "StemHandover.h"
#include "Buses.h"

#include <cmath>
#include <optional>
#include <type_traits>
#include <vector>

namespace
{
	struct RouteEvent
	{
		double time;
		int deck, stem, bus;   // bus 0-3, or AutoDj::offBus
		double positionA, positionB;
	};

	// Two decks played by the Auto-DJ's own commands, 1/20 s per tick, and
	// their stems' buses as the mixer keeps them (Buses.h). After AutoDJ's
	// first routing it checks every tick that each of the buses 1-4 carries
	// exactly one stem -- the one of its number -- and nothing is on AUX.
	struct Rig
	{
		AutoDj dj;
		std::array<AutoDj::DeckView, 2> decks {};
		std::array<double, 2> faders { 0.0, 0.0 };
		std::array<bool, 2> synced {};
		std::array<StemHandover::Envelopes, 2> levels {};
		bool withLevels = false;
		buses::Masks masks = [] { buses::Masks m; m.fill (1u << buses::aux); return m; }();
		std::vector<RouteEvent> routes;
		bool routed = false;
		int ticksOnAux = 0, ticksWithAGapOrTwo = 0, ticksWithAStrayStem = 0;
		std::vector<double> faderMoves;
		std::array<double, 2> endedAt { -1.0, -1.0 };
		double now = 0.0;
		int loads = 0;
		AutoDj::Pick lastPick = AutoDj::Pick::random;
		double trackLength = 240.0, bpm = 120.0;   // ~20 s at 120 = 10 bars
		// Where every loaded track is heard, known this long after its load
		// (0: at once); none: no levels.
		std::optional<double> audibleStart, audibleEnd;
		double levelsAfter = 0.0;
		std::array<double, 2> levelsAt { -1.0, -1.0 };

		AutoDj::Commands tick (double dt = 0.05)
		{
			now += dt;
			for (int i = 0; i < 2; ++i)
				if (levelsAt[(size_t) i] >= 0.0 && now >= levelsAt[(size_t) i])
				{
					auto& d = decks[(size_t) i];
					d.levelsPending = false;
					d.audibleStart = audibleStart;
					d.audibleEnd = audibleEnd;
					levelsAt[(size_t) i] = -1.0;
				}
			for (int i = 0; i < 2; ++i)
			{
				auto& d = decks[(size_t) i];
				if (d.playing)
				{
					d.position += dt * d.rate;
					if (d.position >= d.length)
					{
						d.playing = false;
						endedAt[(size_t) i] = now;
					}
				}
			}

			const auto c = dj.update (decks);
			if (c.load >= 0)
			{
				++loads;
				lastPick = c.loadPick;
				auto& d = decks[(size_t) c.load];
				d.loaded = true;
				d.playing = false;
				d.position = 0.0;
				d.length = trackLength;
				d.gridBpm = bpm;
				d.firstBeat = 0.5;
				d.rate = 1.0;
				d.looping = false;
				d.levels = withLevels ? &levels[(size_t) c.load] : nullptr;
				d.audibleStart.reset();
				d.audibleEnd.reset();
				d.levelsPending = audibleStart || audibleEnd;
				levelsAt[(size_t) c.load] = d.levelsPending ? now + levelsAfter : -1.0;
				if (d.levelsPending && levelsAfter <= 0.0)
				{
					d.levelsPending = false;
					d.audibleStart = audibleStart;
					d.audibleEnd = audibleEnd;
					levelsAt[(size_t) c.load] = -1.0;
				}
			}
			std::vector<buses::Route> wanted;
			for (int d = 0; d < 2; ++d)
				for (int s = 0; s < 4; ++s)
					if (const auto bus = c.bus[(size_t) d][(size_t) s])
					{
						wanted.push_back ({ buses::stemIndex (d, s), *bus == AutoDj::offBus ? buses::off : *bus });
						routes.push_back ({ now, d, s, *bus, decks[0].position, decks[1].position });
					}
			masks = buses::route (masks, wanted);
			routed = routed || ! wanted.empty();
			if (c.start >= 0)
			{
				auto& d = decks[(size_t) c.start];
				d.position = c.startAt;
				d.playing = true;
			}
			if (c.stop >= 0)
				decks[(size_t) c.stop].playing = false;
			if (c.syncOn >= 0) synced[(size_t) c.syncOn] = true;
			if (c.syncOff >= 0) synced[(size_t) c.syncOff] = false;
			for (int d = 0; d < 2; ++d)
				if (c.faderDb[(size_t) d])
				{
					faders[(size_t) d] = *c.faderDb[(size_t) d];
					faderMoves.push_back (*c.faderDb[(size_t) d]);
				}
			if (routed)
				checkTheBuses();
			return c;
		}

		void checkTheBuses()
		{
			for (int bus = 0; bus < 4; ++bus)
			{
				int on = 0;
				for (int stem = 0; stem < buses::stemCount; ++stem)
					if (masks[(size_t) stem] == (1u << bus))
					{
						++on;
						ticksWithAStrayStem += stem % 4 != bus;
					}
				ticksWithAGapOrTwo += on != 1;
			}
			for (auto mask : masks)
				ticksOnAux += (mask & (1u << buses::aux)) != 0;
		}

		void runFor (double seconds) { for (double t = 0; t < seconds; t += 0.05) tick(); }

		// The first time AutoDJ routed this stem of this deck to `bus`.
		std::optional<RouteEvent> firstRoute (int deck, int stem, int bus) const
		{
			for (const auto& e : routes)
				if (e.deck == deck && e.stem == stem && e.bus == bus)
					return e;
			return std::nullopt;
		}

		bool everRouted (int deck, int stem, int bus) const { return firstRoute (deck, stem, bus).has_value(); }
		unsigned maskOf (int deck, int stem) const { return masks[(size_t) buses::stemIndex (deck, stem)]; }
		bool isOn (int deck, int stem) const { return maskOf (deck, stem) == (1u << stem); }
		bool isOff (int deck, int stem) const { return maskOf (deck, stem) == buses::offMask; }
	};

	constexpr int off = AutoDj::offBus;
	constexpr int drums = 0, bass = 1, other = 2, vocals = 3;

	// On a downbeat of a 120 BPM track whose first beat is at 0.5 s, give or
	// take half a tick.
	bool onDownbeat (double position)
	{
		const auto inBar = std::fmod (position - 0.5 + 0.03, 2.0);
		return inBar < 0.06;
	}
}

namespace
{
	// Plays from the start until the next track waits loaded on deck B.
	void playUntilMidTrack (Rig& rig, double seconds = 60.0)
	{
		rig.dj.setEnabled (true);
		rig.runFor (0.2);
		rig.runFor (seconds);
	}

	void tickUntilBPlays (Rig& rig)
	{
		while (! rig.decks[1].playing && rig.decks[0].playing)
			rig.tick();
	}
}

TEST (AutoDj, OffDoesNothing)
{
	Rig rig;
	const auto c = rig.tick();
	EXPECT_EQ (c.load, -1);
	EXPECT_EQ (rig.dj.phase(), AutoDj::Phase::off);
}

TEST (AutoDj, NothingPlayingStartsATrackOnA)
{
	Rig rig;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	EXPECT_TRUE (rig.decks[0].playing);
	EXPECT_FALSE (rig.decks[1].playing);
	EXPECT_EQ (rig.dj.playingDeck(), 0);
	EXPECT_DOUBLE_EQ (rig.faders[0], 0.0);
}

TEST (AutoDj, APlayingDeckIsTakenAsItIs)
{
	Rig rig;
	rig.decks[1] = { true, true, 50.0, 240.0, 120.0, 0.0, 1.0 };
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	EXPECT_EQ (rig.loads, 0);
	EXPECT_EQ (rig.dj.playingDeck(), 1);
}

TEST (AutoDj, TheNextTrackIsLoadedAheadWithItsFaderAtUnity)
{
	Rig rig;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	rig.runFor (240.0 - 20.0 - 30.0 - 5.0);
	EXPECT_EQ (rig.loads, 1) << "not yet";
	rig.runFor (10.0);
	EXPECT_EQ (rig.loads, 2);
	EXPECT_TRUE (rig.decks[1].loaded);
	EXPECT_FALSE (rig.decks[1].playing);
	EXPECT_DOUBLE_EQ (rig.faders[1], 0.0);
}

TEST (AutoDj, TheMixStartsOnADownbeatSyncedAndEndsWithTheTrack)
{
	Rig rig;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	while (! rig.decks[1].playing && rig.decks[0].playing)
		rig.tick();

	ASSERT_TRUE (rig.decks[1].playing);
	EXPECT_TRUE (rig.synced[1]);
	EXPECT_EQ (rig.dj.phase(), AutoDj::Phase::mixing);
	const auto inBar = std::fmod (rig.decks[0].position - 0.5, 2.0);
	EXPECT_LT (inBar, 0.1) << "on the 1 of a bar of the playing track";
	EXPECT_LE (rig.decks[0].length - rig.decks[0].position, 20.0 + 0.1) << "10 bars (20 s) before the end";
	EXPECT_GT (rig.decks[0].length - rig.decks[0].position, 18.0 - 0.1) << "not later than one bar";
	EXPECT_NEAR (rig.decks[1].position, 0.5, 0.06) << "from its own first downbeat";

	rig.runFor (21.0);
	EXPECT_FALSE (rig.decks[0].playing) << "the old deck stopped";
	EXPECT_TRUE (rig.decks[1].playing);
	EXPECT_FALSE (rig.synced[1]) << "SYNC off once it plays alone";
	EXPECT_EQ (rig.dj.playingDeck(), 1);
	EXPECT_EQ (rig.dj.phase(), AutoDj::Phase::playing);
}

TEST (AutoDj, TheFadersStayAtUnity)
{
	Rig rig;
	rig.dj.setEnabled (true);
	rig.runFor (240.0 * 2);
	ASSERT_GE (rig.loads, 3);
	for (auto db : rig.faderMoves)
		EXPECT_DOUBLE_EQ (db, 0.0);
	EXPECT_DOUBLE_EQ (rig.faders[0], 0.0);
	EXPECT_DOUBLE_EQ (rig.faders[1], 0.0);
}

TEST (AutoDj, TheNewTrackStartsWithOnlyItsDrumsOnBusOne)
{
	Rig rig;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	tickUntilBPlays (rig);
	ASSERT_EQ (rig.dj.phase(), AutoDj::Phase::mixing);
	EXPECT_TRUE (rig.isOn (1, drums)) << "bus 1: the new drums";
	EXPECT_TRUE (rig.isOff (0, drums)) << "the old drums left on the same downbeat";
	for (auto stem : { bass, other, vocals })
	{
		EXPECT_TRUE (rig.isOn (0, stem)) << "bus " << stem + 1 << " still the old track's";
		EXPECT_TRUE (rig.isOff (1, stem)) << "the new " << stem << " on no bus yet";
	}
}

TEST (AutoDj, BassAndOtherChangeOverOneAtATimeOnDownbeats)
{
	Rig rig;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	tickUntilBPlays (rig);
	const auto mixStart = rig.now;
	rig.runFor (22.0);

	for (auto stem : { bass, other })
	{
		const auto out = rig.firstRoute (0, stem, off), in = rig.firstRoute (1, stem, stem);
		ASSERT_TRUE (out && in) << "stem " << stem;
		EXPECT_DOUBLE_EQ (out->time, in->time) << "the bus switches source on one tick: never two, never a gap";
		EXPECT_GT (in->time, mixStart);
		EXPECT_LT (in->time, rig.endedAt[0]) << "while the old track still plays";
		EXPECT_TRUE (onDownbeat (in->positionB)) << "on a downbeat, at " << in->positionB;
	}

	const auto bassAt = rig.firstRoute (1, bass, bass)->time, otherAt = rig.firstRoute (1, other, other)->time;
	EXPECT_LT (bassAt, otherAt) << "bass first";
	EXPECT_GE (otherAt - bassAt, 2 * 2.0 - 0.1) << "spread, not bunched";
	EXPECT_GE (bassAt - mixStart, 2 * 2.0 - 0.1) << "not on the drums' heels";
	EXPECT_EQ (rig.ticksWithAGapOrTwo, 0);
	EXPECT_EQ (rig.ticksWithAStrayStem, 0) << "bus n only ever carries a stem n";
}

TEST (AutoDj, TheNewVocalsComeOnTheFirstDownbeatAfterTheOldTrackEnds)
{
	Rig rig;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	tickUntilBPlays (rig);
	rig.runFor (22.0);

	ASSERT_GT (rig.endedAt[0], 0.0);
	const auto in = rig.firstRoute (1, vocals, vocals);
	ASSERT_TRUE (in);
	EXPECT_GE (in->time, rig.endedAt[0]) << "never before the old track ends";
	EXPECT_LT (in->time - rig.endedAt[0], 2.0) << "within the bar after it";
	EXPECT_TRUE (onDownbeat (in->positionB)) << "on a downbeat, at " << in->positionB;
	const auto out = rig.firstRoute (0, vocals, off);
	ASSERT_TRUE (out);
	EXPECT_DOUBLE_EQ (out->time, in->time) << "the old vocals sing to the end";
}

TEST (AutoDj, TheOldDeckStopsOnNoBus)
{
	Rig rig;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	tickUntilBPlays (rig);
	rig.runFor (22.0);
	ASSERT_EQ (rig.dj.playingDeck(), 1);
	for (int stem = 0; stem < 4; ++stem)
	{
		EXPECT_TRUE (rig.isOff (0, stem));
		EXPECT_TRUE (rig.isOn (1, stem));
	}
}

TEST (AutoDj, NothingIsOnAuxWhileItPlays)
{
	Rig rig;
	rig.dj.setEnabled (true);
	rig.runFor (240.0 * 3);
	ASSERT_GE (rig.loads, 4);
	EXPECT_TRUE (rig.routed);
	EXPECT_EQ (rig.ticksOnAux, 0);
	EXPECT_EQ (rig.ticksWithAGapOrTwo, 0) << "every bus 1-4 always carries exactly one stem";
	EXPECT_EQ (rig.ticksWithAStrayStem, 0);
}

TEST (AutoDj, TheFirstTrackTakesTheBusesAsItStarts)
{
	Rig rig;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	ASSERT_TRUE (rig.decks[0].playing);
	for (int stem = 0; stem < 4; ++stem)
	{
		EXPECT_TRUE (rig.isOn (0, stem)) << "stem " << stem + 1 << " on bus " << stem + 1;
		EXPECT_TRUE (rig.isOff (1, stem));
	}
}

TEST (AutoDj, APlayingDeckIsTakenOntoTheBusesWhenItGoesOn)
{
	Rig rig;
	rig.decks[1] = { true, true, 50.0, 240.0, 120.0, 0.0, 1.0 };
	rig.dj.setEnabled (true);
	rig.tick();
	for (int stem = 0; stem < 4; ++stem)
	{
		EXPECT_TRUE (rig.isOn (1, stem));
		EXPECT_TRUE (rig.isOff (0, stem));
	}
}

TEST (AutoDj, TheIncomingStemsAreOnNoBusUntilTheirSwitch)
{
	Rig rig;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	rig.runFor (240.0 - 40.0);   // the next one waits on B
	for (int stem = 0; stem < 4; ++stem)
		EXPECT_TRUE (rig.isOff (1, stem)) << "loaded, silent, not on AUX";
	tickUntilBPlays (rig);
	std::array<bool, 4> seenOn {};
	for (double t = 0; t < 22.0; t += 0.05)
	{
		rig.tick();
		for (int stem = 0; stem < 4; ++stem)
		{
			const auto switched = rig.everRouted (1, stem, stem);
			if (! switched)
				EXPECT_TRUE (rig.isOff (1, stem)) << "stem " << stem << " before its switch";
			seenOn[(size_t) stem] = seenOn[(size_t) stem] || switched;
		}
	}
	EXPECT_EQ (seenOn, (std::array<bool, 4> { true, true, true, true }));
}

TEST (AutoDj, TheOverlapIsAboutTwentySecondsInWholeBars)
{
	// 60 BPM: 5 bars of 4 s.
	Rig slow;
	slow.bpm = 60.0;
	slow.dj.setEnabled (true);
	slow.runFor (0.2);
	tickUntilBPlays (slow);
	ASSERT_TRUE (slow.decks[1].playing);
	EXPECT_LE (slow.decks[0].length - slow.decks[0].position, 20.0 + 0.1);
	EXPECT_GT (slow.decks[0].length - slow.decks[0].position, 16.0 - 0.1);

	// 30 BPM: 20 s would be 2.5 bars, so at least 4 bars (32 s).
	Rig crawl;
	crawl.bpm = 30.0;
	crawl.dj.setEnabled (true);
	crawl.runFor (0.2);
	tickUntilBPlays (crawl);
	ASSERT_TRUE (crawl.decks[1].playing);
	EXPECT_LE (crawl.decks[0].length - crawl.decks[0].position, 32.0 + 0.1);
	EXPECT_GT (crawl.decks[0].length - crawl.decks[0].position, 24.0 - 0.1);
}

TEST (AutoDj, TheOverlapCountsBarsAtThePlayingTempo)
{
	// Tempo fader up to 1.6: a bar of 2 track seconds lasts 1.25 s, so 16
	// bars make the 20 s -- 32 seconds of the track.
	Rig rig;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	rig.decks[0].rate = 1.6;
	tickUntilBPlays (rig);
	ASSERT_TRUE (rig.decks[1].playing);
	EXPECT_LE (rig.decks[0].length - rig.decks[0].position, 32.0 + 0.1);
	EXPECT_GT (rig.decks[0].length - rig.decks[0].position, 30.0 - 0.1);
}

TEST (AutoDj, TheAnalysisPutsTheBassChangeoverOnTheOldBassRest)
{
	Rig rig;
	rig.withLevels = true;
	for (auto& deck : rig.levels)
		for (auto& stem : deck)
			stem.rms.assign ((size_t) (240.0 / stem.hop), 0.5f);
	// The mix starts at 220.5 (19.5 s before the end); the old bass rests
	// for the bar from downbeat 6 of the overlap.
	auto& oldBass = rig.levels[0][(size_t) bass];
	for (auto i = (size_t) ((220.5 + 12.0) / oldBass.hop); i < (size_t) ((220.5 + 14.0) / oldBass.hop); ++i)
		oldBass.rms[i] = 0.0f;

	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	tickUntilBPlays (rig);
	ASSERT_NEAR (rig.decks[0].position, 220.5, 0.06);
	rig.runFor (22.0);

	const auto in = rig.firstRoute (1, bass, bass);
	ASSERT_TRUE (in);
	EXPECT_NEAR (in->positionB, 0.5 + 6 * 2.0, 0.06) << "on downbeat 6";
}

// The DJ's mute buttons are his alone: AutoDJ has no command that mutes.
namespace
{
	template <typename C, typename = void>
	struct HasMute : std::false_type {};
	template <typename C>
	struct HasMute<C, std::void_t<decltype (std::declval<C>().mute)>> : std::true_type {};
}

TEST (AutoDj, TheMutesAreTheDjsAlone)
{
	EXPECT_FALSE (HasMute<AutoDj::Commands>::value);
}

TEST (AutoDj, ItGoesOnTrackAfterTrack)
{
	Rig rig;
	rig.dj.setEnabled (true);
	rig.runFor (240.0 * 3);
	EXPECT_GE (rig.loads, 4);
	EXPECT_TRUE (rig.decks[0].playing || rig.decks[1].playing);
}

TEST (AutoDj, WithoutAGridItMixesUnsynced)
{
	Rig rig;
	rig.bpm = 0.0;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	while (! rig.decks[1].playing && rig.decks[0].playing)
		rig.tick();
	EXPECT_FALSE (rig.synced[1]);
	EXPECT_LE (rig.decks[0].length - rig.decks[0].position, AutoDj::defaultNoGridMixSeconds + 0.1);
}

TEST (AutoDj, NothingIsLoadedOverALoop)
{
	Rig rig;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	rig.decks[1] = { true, false, 20.0, 240.0, 120.0, 0.5, 1.0, true };   // the DJ loops on B
	const auto loads = rig.loads;
	rig.runFor (240.0 - 20.0 - 20.0);
	EXPECT_EQ (rig.loads, loads) << "B keeps its loop";
	rig.decks[1].looping = false;
	rig.runFor (0.2);
	EXPECT_EQ (rig.loads, loads + 1) << "loop off: now it loads";
}

TEST (AutoDj, TheMixWaitsWhileThePlayingDeckLoops)
{
	Rig rig;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	rig.runFor (240.0 - 40.0);          // the next one is loaded
	rig.decks[0].looping = true;
	rig.runFor (20.0);
	EXPECT_FALSE (rig.decks[1].playing) << "no mix while A loops";
	rig.decks[0].looping = false;
	while (! rig.decks[1].playing && rig.decks[0].playing)
		rig.tick();
	EXPECT_TRUE (rig.decks[1].playing) << "loop off: the mix starts";
}

TEST (AutoDj, WithoutAGridTheStemsChangeInFourStepsOverTheMixTime)
{
	Rig rig;
	rig.bpm = 0.0;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	tickUntilBPlays (rig);
	const auto start = rig.now;
	EXPECT_TRUE (rig.isOn (1, drums)) << "step 0: the drums swapped";
	EXPECT_TRUE (rig.isOff (0, drums));
	for (auto stem : { bass, other, vocals })
		EXPECT_TRUE (rig.isOn (0, stem) && rig.isOff (1, stem));
	rig.runFor (12.0);

	const auto t = AutoDj::defaultNoGridMixSeconds;
	const std::array<double, 3> at { t / 3.0, 2.0 * t / 3.0, t };
	for (auto stem : { bass, other, vocals })
	{
		const auto out = rig.firstRoute (0, stem, off), in = rig.firstRoute (1, stem, stem);
		ASSERT_TRUE (out && in) << "stem " << stem;
		EXPECT_DOUBLE_EQ (out->time, in->time) << "stem " << stem << " swapped at once";
		EXPECT_NEAR (in->time - start, at[(size_t) stem - 1], 0.11) << "stem " << stem;
	}
	EXPECT_FALSE (rig.decks[0].playing) << "the old deck stopped with the vocals";
	EXPECT_EQ (rig.dj.playingDeck(), 1);
	EXPECT_EQ (rig.ticksOnAux, 0);
	EXPECT_EQ (rig.ticksWithAGapOrTwo, 0);
	for (auto db : rig.faderMoves)
		EXPECT_DOUBLE_EQ (db, 0.0);
}

TEST (AutoDj, TheMixLengthIsASetting)
{
	Rig rig;
	rig.dj.setMixLength ({ 4, 10.0 });   // 4 bars at 120 = 8 s
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	while (! rig.decks[1].playing && rig.decks[0].playing)
		rig.tick();
	ASSERT_TRUE (rig.decks[1].playing);
	EXPECT_LE (rig.decks[0].length - rig.decks[0].position, 8.0 + 0.1) << "4 bars before the end";
	EXPECT_GT (rig.decks[0].length - rig.decks[0].position, 8.0 - 2.1) << "not later than one bar";
}

TEST (AutoDj, TheNoGridMixLengthIsASetting)
{
	Rig rig;
	rig.bpm = 0.0;
	rig.dj.setMixLength ({ 16, 20.0 });
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	while (! rig.decks[1].playing && rig.decks[0].playing)
		rig.tick();
	ASSERT_TRUE (rig.decks[1].playing);
	EXPECT_NEAR (rig.decks[0].length - rig.decks[0].position, 20.0, 0.1);
}

TEST (AutoDj, ANewMixLengthAppliesToTheNextMixNotTheRunningOne)
{
	Rig rig;
	rig.dj.setMixLength ({ 16, 10.0 });
	playUntilMidTrack (rig);
	ASSERT_TRUE (rig.dj.requestMixNow (AutoDj::Pick::next));
	tickUntilBPlays (rig);
	ASSERT_EQ (rig.dj.phase(), AutoDj::Phase::mixing);
	rig.dj.setMixLength ({ 4, 10.0 });
	rig.runFor (16.0);
	EXPECT_EQ (rig.dj.phase(), AutoDj::Phase::mixing) << "the running mix keeps its 16 bars";
	EXPECT_NEAR (rig.dj.mixProgress(), 0.5, 0.02);
}

TEST (AutoDj, TheMixLengthIsAboutTwentySecondsUnlessBarsAreSet)
{
	AutoDj dj;
	EXPECT_EQ (dj.mixLength().bars, AutoDj::aboutTwentySeconds);
	dj.setMixLength ({ 2, 10.0 });
	EXPECT_EQ (dj.mixLength().bars, 4) << "at least 4 bars";
	dj.setMixLength ({ 8, 10.0 });
	EXPECT_EQ (dj.mixLength().bars, 8);
}

TEST (AutoDj, AMixLengthOutOfRangeIsHeldToASaneOne)
{
	AutoDj dj;
	dj.setMixLength ({ -3, -3.0 });
	EXPECT_EQ (dj.mixLength().bars, AutoDj::aboutTwentySeconds);
	EXPECT_GT (dj.mixLength().noGridSeconds, 0.0);
}

TEST (AutoDj, NextDuringPlayLoadsTheNextTrackAndMixesOnTheNextDownbeat)
{
	Rig rig;
	rig.dj.setMixLength ({ 8, 10.0 });   // 8 bars at 120 = 16 s
	playUntilMidTrack (rig);
	ASSERT_TRUE (rig.dj.canMixNow());
	const auto loads = rig.loads;

	EXPECT_TRUE (rig.dj.requestMixNow (AutoDj::Pick::next));
	rig.tick();
	EXPECT_EQ (rig.loads, loads + 1);
	EXPECT_EQ (rig.lastPick, AutoDj::Pick::next) << "the next track, not a random one";
	EXPECT_DOUBLE_EQ (rig.faders[1], 0.0);

	const auto pressedAt = rig.decks[0].position;
	tickUntilBPlays (rig);
	ASSERT_TRUE (rig.decks[1].playing);
	EXPECT_EQ (rig.dj.phase(), AutoDj::Phase::mixing);
	EXPECT_TRUE (rig.synced[1]) << "beat-matched as its own mix";
	EXPECT_LE (rig.decks[0].position - pressedAt, 2.0 + 0.1) << "within one bar of the press";
	EXPECT_LT (std::fmod (rig.decks[0].position - 0.5, 2.0), 0.1) << "on the 1 of a bar";
	EXPECT_NEAR (rig.decks[1].position, 0.5, 0.06);

	rig.runFor (8.0);
	EXPECT_NEAR (rig.dj.mixProgress(), 0.5, 0.02) << "over the configured 8 bars";
	rig.runFor (8.2);
	EXPECT_FALSE (rig.decks[0].playing);
	EXPECT_EQ (rig.dj.playingDeck(), 1);
	EXPECT_EQ (rig.dj.phase(), AutoDj::Phase::playing);
}

TEST (AutoDj, NextSwapsTheVocalsAndStopsTheOldDeckAtTheOverlapsEnd)
{
	Rig rig;
	playUntilMidTrack (rig);
	ASSERT_TRUE (rig.dj.requestMixNow (AutoDj::Pick::next));
	tickUntilBPlays (rig);
	const auto start = rig.now;
	rig.runFor (21.0);

	const auto vocalsIn = rig.firstRoute (1, vocals, vocals);
	ASSERT_TRUE (vocalsIn);
	EXPECT_NEAR (vocalsIn->time - start, 20.0, 0.06) << "10 bars, about 20 s";
	EXPECT_TRUE (onDownbeat (vocalsIn->positionB));
	EXPECT_NEAR (rig.decks[0].position, vocalsIn->positionA, 1e-9) << "the old deck stopped with it";
	EXPECT_FALSE (rig.decks[0].playing);
	for (auto stem : { bass, other })
	{
		const auto in = rig.firstRoute (1, stem, stem);
		ASSERT_TRUE (in);
		EXPECT_LT (in->time, vocalsIn->time);
		EXPECT_GT (in->time, start);
	}
	for (int stem = 0; stem < 4; ++stem)
		EXPECT_TRUE (rig.isOff (0, stem));
}

TEST (AutoDj, PreviousDuringPlayAsksForThePreviousTrack)
{
	Rig rig;
	playUntilMidTrack (rig);
	EXPECT_TRUE (rig.dj.requestMixNow (AutoDj::Pick::previous));
	rig.tick();
	EXPECT_EQ (rig.lastPick, AutoDj::Pick::previous);
}

TEST (AutoDj, NextReplacesATrackAlreadyWaiting)
{
	Rig rig;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	rig.runFor (240.0 - 40.0);   // the random next one is loaded on B
	ASSERT_EQ (rig.dj.phase(), AutoDj::Phase::ready);
	const auto loads = rig.loads;
	EXPECT_TRUE (rig.dj.requestMixNow (AutoDj::Pick::next));
	rig.tick();
	EXPECT_EQ (rig.loads, loads + 1);
	EXPECT_EQ (rig.lastPick, AutoDj::Pick::next);
}

TEST (AutoDj, NextWithoutAGridMixesNowOverTheSeconds)
{
	Rig rig;
	rig.bpm = 0.0;
	rig.dj.setMixLength ({ 16, 6.0 });
	playUntilMidTrack (rig);
	const auto pressedAt = rig.decks[0].position;
	ASSERT_TRUE (rig.dj.requestMixNow (AutoDj::Pick::next));
	tickUntilBPlays (rig);
	ASSERT_TRUE (rig.decks[1].playing);
	EXPECT_LE (rig.decks[0].position - pressedAt, 0.3) << "no downbeat to wait for";
	EXPECT_FALSE (rig.synced[1]);
	rig.runFor (3.0);
	EXPECT_NEAR (rig.dj.mixProgress(), 0.5, 0.03);
	rig.runFor (3.2);
	EXPECT_EQ (rig.dj.playingDeck(), 1);
}

TEST (AutoDj, APressDuringAMixIsIgnored)
{
	Rig rig;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	tickUntilBPlays (rig);
	ASSERT_EQ (rig.dj.phase(), AutoDj::Phase::mixing);
	EXPECT_FALSE (rig.dj.canMixNow());
	const auto loads = rig.loads;
	EXPECT_FALSE (rig.dj.requestMixNow (AutoDj::Pick::next));
	rig.runFor (1.0);
	EXPECT_EQ (rig.loads, loads);
	EXPECT_EQ (rig.dj.phase(), AutoDj::Phase::mixing);
}

TEST (AutoDj, NoMixNowWhileOff)
{
	AutoDj dj;
	EXPECT_FALSE (dj.canMixNow());
	EXPECT_FALSE (dj.requestMixNow (AutoDj::Pick::next));
}

TEST (AutoDj, AMixNowStillWaitsForTheDjsLoop)
{
	Rig rig;
	playUntilMidTrack (rig);
	rig.decks[0].looping = true;
	ASSERT_TRUE (rig.dj.requestMixNow (AutoDj::Pick::next));
	rig.runFor (5.0);
	EXPECT_FALSE (rig.decks[1].playing) << "no mix while A loops";
	rig.decks[0].looping = false;
	rig.runFor (2.2);
	EXPECT_TRUE (rig.decks[1].playing) << "loop off: the mix starts at the next downbeat";
}

// Where a track is heard, not where its file starts and ends.
TEST (AutoDj, TheOldTrackEndsWhereItIsLastHeard)
{
	Rig rig;
	rig.audibleEnd = 220.0;   // 20 s of silence after it
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	tickUntilBPlays (rig);
	ASSERT_TRUE (rig.decks[1].playing);
	EXPECT_LE (220.0 - rig.decks[0].position, 20.0 + 0.1) << "the overlap ends with the sound, not the file";
	EXPECT_GT (220.0 - rig.decks[0].position, 18.0 - 0.1);
	rig.runFor (22.0);

	const auto vocals = rig.firstRoute (1, 3, 3);
	ASSERT_TRUE (vocals);
	EXPECT_NEAR (vocals->positionA, 220.5, 0.06) << "on the downbeat that closes the bar the sound ends in";
	EXPECT_LT (rig.endedAt[0], 0.0) << "it never ran into its silence";
	EXPECT_FALSE (rig.decks[0].playing) << "the old deck stopped with the vocals";
}

TEST (AutoDj, TheNextTrackIsLoadedAheadOfTheAudibleEnd)
{
	Rig rig;
	rig.audibleEnd = 200.0;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	rig.runFor (200.0 - 20.0 - 30.0 - 2.0);
	EXPECT_EQ (rig.loads, 1) << "not yet";
	rig.runFor (4.0);
	EXPECT_EQ (rig.loads, 2);
}

TEST (AutoDj, TheNewTrackStartsOnTheFirstDownbeatOfItsSound)
{
	// Downbeats at 0.5, 2.5, ... 8.5, 10.5: the sound from 9.4 is not on a
	// downbeat, nor in the last beat before one.
	Rig rig;
	rig.audibleStart = 9.4;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	EXPECT_NEAR (rig.decks[0].position, 10.5, 0.2) << "the first track too";
	tickUntilBPlays (rig);
	EXPECT_NEAR (rig.decks[1].position, 10.5, 0.06) << "not the file's start, not a downbeat in the silence";
	EXPECT_TRUE (rig.synced[1]);
	rig.runFor (22.0);
	const auto bass = rig.firstRoute (1, 1, 1);
	ASSERT_TRUE (bass);
	EXPECT_TRUE (onDownbeat (bass->positionB)) << "its bars count from there";
}

TEST (AutoDj, APickupIsHeardFromTheDownbeatBeforeIt)
{
	// Sound from 10.2, in the last beat before the downbeat at 10.5.
	Rig rig;
	rig.audibleStart = 10.2;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	tickUntilBPlays (rig);
	EXPECT_NEAR (rig.decks[1].position, 8.5, 0.06);
}

TEST (AutoDj, SoundRightOnADownbeatStartsThere)
{
	// The level crosses a little after the hit: still that downbeat.
	Rig rig;
	rig.audibleStart = 10.55;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	tickUntilBPlays (rig);
	EXPECT_NEAR (rig.decks[1].position, 10.5, 0.06);
}

TEST (AutoDj, WithoutAGridTheAudibleSecondsCount)
{
	Rig rig;
	rig.bpm = 0.0;
	rig.audibleStart = 3.7;
	rig.audibleEnd = 225.0;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	tickUntilBPlays (rig);
	EXPECT_NEAR (rig.decks[1].position, 3.7, 0.06) << "from where it is heard";
	EXPECT_NEAR (225.0 - rig.decks[0].position, AutoDj::defaultNoGridMixSeconds, 0.1);
	rig.runFor (11.0);
	EXPECT_FALSE (rig.decks[0].playing);
	EXPECT_LT (rig.endedAt[0], 0.0) << "stopped at its audible end";
	EXPECT_NEAR (rig.decks[0].position, 225.0, 0.1);
}

TEST (AutoDj, NextWaitsForTheNewTracksLevels)
{
	Rig rig;
	playUntilMidTrack (rig);
	rig.audibleStart = 9.4;
	rig.levelsAfter = 3.0;   // longer than the bar to the next downbeat
	ASSERT_TRUE (rig.dj.requestMixNow (AutoDj::Pick::next));
	const auto pressed = rig.now;
	tickUntilBPlays (rig);
	EXPECT_GE (rig.now - pressed, 3.0) << "the mix waited for them";
	EXPECT_LE (rig.now - pressed, 3.0 + 2.1) << "then the next downbeat";
	EXPECT_NEAR (rig.decks[1].position, 10.5, 0.06) << "and used them";
	EXPECT_LT (std::fmod (rig.decks[0].position - 0.5, 2.0), 0.1) << "on the 1 of a bar of the playing track";
}

TEST (AutoDj, LevelsThatNeverComeAreNotWaitedForLong)
{
	Rig rig;
	playUntilMidTrack (rig);
	rig.audibleStart = 9.4;
	rig.levelsAfter = 1000.0;
	ASSERT_TRUE (rig.dj.requestMixNow (AutoDj::Pick::next));
	const auto pressed = rig.now;
	tickUntilBPlays (rig);
	EXPECT_LE (rig.now - pressed, AutoDj::maxLevelsWait + 2.1);
	EXPECT_NEAR (rig.decks[1].position, 0.5, 0.06) << "without them: from the first downbeat, as before";
}
