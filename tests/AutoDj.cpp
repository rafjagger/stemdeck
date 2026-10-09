#include <gtest/gtest.h>

#include "AutoDj.h"
#include "StemHandover.h"

#include <cmath>
#include <optional>
#include <vector>

namespace
{
	struct MuteEvent
	{
		double time;
		int deck, stem;
		bool muted;
		double positionA, positionB;
	};

	// Two decks played by the Auto-DJ's own commands, 1/20 s per tick.
	// Mutes persist across a load, as the player's do.
	struct Rig
	{
		AutoDj dj;
		std::array<AutoDj::DeckView, 2> decks {};
		std::array<double, 2> faders { 0.0, 0.0 };
		std::array<bool, 2> synced {};
		std::array<StemHandover::Envelopes, 2> levels {};
		bool withLevels = false;
		std::vector<MuteEvent> mutes;
		std::vector<double> faderMoves;
		std::array<double, 2> endedAt { -1.0, -1.0 };
		double now = 0.0;
		int loads = 0;
		AutoDj::Pick lastPick = AutoDj::Pick::random;
		double trackLength = 240.0, bpm = 120.0;   // ~20 s at 120 = 10 bars

		AutoDj::Commands tick (double dt = 0.05)
		{
			now += dt;
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
			}
			for (int d = 0; d < 2; ++d)
				for (int s = 0; s < 4; ++s)
					if (const auto m = c.mute[(size_t) d][(size_t) s])
					{
						decks[(size_t) d].muted[(size_t) s] = *m;
						mutes.push_back ({ now, d, s, *m, decks[0].position, decks[1].position });
					}
			if (c.start >= 0)
			{
				auto& d = decks[(size_t) c.start];
				d.position = d.firstBeat;
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
			return c;
		}

		void runFor (double seconds) { for (double t = 0; t < seconds; t += 0.05) tick(); }

		// The first time AutoDJ set this stem of this deck to `muted`.
		std::optional<MuteEvent> firstMute (int deck, int stem, bool muted) const
		{
			for (const auto& e : mutes)
				if (e.deck == deck && e.stem == stem && e.muted == muted)
					return e;
			return std::nullopt;
		}

		bool everMuted (int deck, int stem, bool muted) const { return firstMute (deck, stem, muted).has_value(); }
	};

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

TEST (AutoDj, TheNewTrackStartsWithOnlyItsDrumsAndTheOldDrumsGo)
{
	Rig rig;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	tickUntilBPlays (rig);
	ASSERT_EQ (rig.dj.phase(), AutoDj::Phase::mixing);
	const std::array<bool, 4> newStems { false, true, true, true }, oldStems { true, false, false, false };
	EXPECT_EQ (rig.decks[1].muted, newStems) << "only the new drums play";
	EXPECT_EQ (rig.decks[0].muted, oldStems) << "the old drums are out on the same downbeat";
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
		const auto out = rig.firstMute (0, stem, true), in = rig.firstMute (1, stem, false);
		ASSERT_TRUE (out && in) << "stem " << stem;
		EXPECT_DOUBLE_EQ (out->time, in->time) << "old off and new on together: never two, never a gap";
		EXPECT_GT (in->time, mixStart);
		EXPECT_LT (in->time, rig.endedAt[0]) << "while the old track still plays";
		EXPECT_TRUE (onDownbeat (in->positionB)) << "on a downbeat, at " << in->positionB;
	}

	const auto bassAt = rig.firstMute (1, bass, false)->time, otherAt = rig.firstMute (1, other, false)->time;
	EXPECT_LT (bassAt, otherAt) << "bass first";
	EXPECT_GE (otherAt - bassAt, 2 * 2.0 - 0.1) << "spread, not bunched";
	EXPECT_GE (bassAt - mixStart, 2 * 2.0 - 0.1) << "not on the drums' heels";
}

TEST (AutoDj, TheNewVocalsComeOnTheFirstDownbeatAfterTheOldTrackEnds)
{
	Rig rig;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	tickUntilBPlays (rig);
	rig.runFor (22.0);

	ASSERT_GT (rig.endedAt[0], 0.0);
	const auto in = rig.firstMute (1, vocals, false);
	ASSERT_TRUE (in);
	EXPECT_GE (in->time, rig.endedAt[0]) << "never before the old track ends";
	EXPECT_LT (in->time - rig.endedAt[0], 2.0) << "within the bar after it";
	EXPECT_TRUE (onDownbeat (in->positionB)) << "on a downbeat, at " << in->positionB;
	EXPECT_FALSE (rig.everMuted (0, vocals, true)) << "the old vocals sing to the end";
}

TEST (AutoDj, TheOldDeckIsLeftWithoutAutoDjsMutes)
{
	Rig rig;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	tickUntilBPlays (rig);
	rig.runFor (22.0);
	ASSERT_EQ (rig.dj.playingDeck(), 1);
	const std::array<bool, 4> none {};
	EXPECT_EQ (rig.decks[0].muted, none) << "a DJ loading onto it later hears every stem";
	EXPECT_EQ (rig.decks[1].muted, none);
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

	const auto in = rig.firstMute (1, bass, false);
	ASSERT_TRUE (in);
	EXPECT_NEAR (in->positionB, 0.5 + 6 * 2.0, 0.06) << "on downbeat 6";
}

TEST (AutoDj, AStemTheDjMutedStaysMuted)
{
	Rig rig;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	rig.runFor (240.0 - 40.0);   // the next one waits on B
	ASSERT_EQ (rig.dj.phase(), AutoDj::Phase::ready);
	rig.decks[1].muted[(size_t) vocals] = true;   // by hand, on the waiting track
	rig.decks[0].muted[(size_t) other] = true;    // and on the playing one
	tickUntilBPlays (rig);
	rig.runFor (22.0);
	ASSERT_EQ (rig.dj.playingDeck(), 1);

	EXPECT_TRUE (rig.decks[1].muted[(size_t) vocals]) << "AutoDJ never unmutes what the DJ muted";
	EXPECT_FALSE (rig.everMuted (1, vocals, false));
	EXPECT_TRUE (rig.decks[0].muted[(size_t) other]) << "nor clears it from the old deck";
	EXPECT_FALSE (rig.decks[0].muted[(size_t) bass]) << "its own mutes it does clear";
	EXPECT_FALSE (rig.decks[0].muted[(size_t) drums]);
	EXPECT_TRUE (rig.firstMute (1, other, false)) << "the new other still comes in";
}

TEST (AutoDj, AStemTheDjUnmutesDuringTheMixIsHis)
{
	Rig rig;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	tickUntilBPlays (rig);
	rig.tick();
	rig.decks[1].muted[(size_t) vocals] = false;   // the DJ wants the new vocals now
	rig.runFor (22.0);
	EXPECT_FALSE (rig.decks[1].muted[(size_t) vocals]);
	EXPECT_FALSE (rig.everMuted (1, vocals, false)) << "nothing left for AutoDJ to unmute";
	int mutedAgain = 0;
	for (const auto& e : rig.mutes)
		mutedAgain += e.deck == 1 && e.stem == vocals && e.muted;
	EXPECT_EQ (mutedAgain, 1) << "only the mute at the start, never again";
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
	const std::array<bool, 4> onlyDrums { false, true, true, true }, none {};
	EXPECT_EQ (rig.decks[1].muted, onlyDrums) << "step 0: the new drums in";
	EXPECT_EQ (rig.decks[0].muted, none) << "over the old track, whole";
	rig.runFor (12.0);

	const auto t = AutoDj::defaultNoGridMixSeconds;
	const auto bassOut = rig.firstMute (0, bass, true), bassIn = rig.firstMute (1, bass, false);
	ASSERT_TRUE (bassOut && bassIn);
	EXPECT_DOUBLE_EQ (bassOut->time, bassIn->time) << "step 1: the bass swapped at once";
	EXPECT_NEAR (bassIn->time - start, t / 3.0, 0.06);

	const auto drumsOut = rig.firstMute (0, drums, true), otherIn = rig.firstMute (1, other, false);
	ASSERT_TRUE (drumsOut && otherIn);
	EXPECT_DOUBLE_EQ (drumsOut->time, otherIn->time) << "step 2: old drums out, new other in";
	EXPECT_NEAR (otherIn->time - start, 2.0 * t / 3.0, 0.06);

	const auto vocalsIn = rig.firstMute (1, vocals, false);
	ASSERT_TRUE (vocalsIn);
	EXPECT_NEAR (vocalsIn->time - start, t, 0.11) << "step 3: the vocals, and the old deck stops";
	EXPECT_FALSE (rig.decks[0].playing);
	EXPECT_EQ (rig.decks[0].muted, none);
	EXPECT_EQ (rig.dj.playingDeck(), 1);
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

	const auto vocalsIn = rig.firstMute (1, vocals, false);
	ASSERT_TRUE (vocalsIn);
	EXPECT_NEAR (vocalsIn->time - start, 20.0, 0.06) << "10 bars, about 20 s";
	EXPECT_TRUE (onDownbeat (vocalsIn->positionB));
	EXPECT_NEAR (rig.decks[0].position, vocalsIn->positionA, 1e-9) << "the old deck stopped with it";
	EXPECT_FALSE (rig.decks[0].playing);
	for (auto stem : { bass, other })
	{
		const auto in = rig.firstMute (1, stem, false);
		ASSERT_TRUE (in);
		EXPECT_LT (in->time, vocalsIn->time);
		EXPECT_GT (in->time, start);
	}
	const std::array<bool, 4> none {};
	EXPECT_EQ (rig.decks[0].muted, none);
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
