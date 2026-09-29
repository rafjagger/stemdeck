#include <gtest/gtest.h>

#include "AutoDj.h"

#include <cmath>

namespace
{
	// Two decks played by the Auto-DJ's own commands, 1/20 s per tick.
	struct Rig
	{
		AutoDj dj;
		std::array<AutoDj::DeckView, 2> decks {};
		std::array<double, 2> faders { 0.0, 0.0 };
		std::array<bool, 2> synced {};
		int loads = 0;
		double trackLength = 240.0, bpm = 120.0;   // 16 bars at 120 = 32 s

		AutoDj::Commands tick (double dt = 0.05)
		{
			for (auto& d : decks)
				if (d.playing)
				{
					d.position += dt * d.rate;
					if (d.position >= d.length)
						d.playing = false;
				}

			const auto c = dj.update (decks);
			if (c.load >= 0)
			{
				++loads;
				decks[(size_t) c.load] = { true, false, 0.0, trackLength, bpm, 0.5, 1.0 };
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
					faders[(size_t) d] = *c.faderDb[(size_t) d];
			return c;
		}

		void runFor (double seconds) { for (double t = 0; t < seconds; t += 0.05) tick(); }
	};
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

TEST (AutoDj, TheNextTrackIsLoadedAheadWithItsFaderDown)
{
	Rig rig;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	rig.runFor (240.0 - 32.0 - 30.0 - 5.0);
	EXPECT_EQ (rig.loads, 1) << "not yet";
	rig.runFor (10.0);
	EXPECT_EQ (rig.loads, 2);
	EXPECT_TRUE (rig.decks[1].loaded);
	EXPECT_FALSE (rig.decks[1].playing);
	EXPECT_DOUBLE_EQ (rig.faders[1], AutoDj::silentDb);
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
	EXPECT_LE (rig.decks[0].length - rig.decks[0].position, 32.0 + 0.1) << "16 bars before the end";
	EXPECT_NEAR (rig.decks[1].position, 0.5, 0.06) << "from its own first downbeat";

	rig.runFor (16.0);   // half way
	EXPECT_GT (rig.faders[1], -6.0);
	EXPECT_LT (rig.faders[0], -1.0);

	rig.runFor (17.0);
	EXPECT_FALSE (rig.decks[0].playing) << "the old deck stopped";
	EXPECT_TRUE (rig.decks[1].playing);
	EXPECT_DOUBLE_EQ (rig.faders[1], 0.0);
	EXPECT_FALSE (rig.synced[1]) << "SYNC off once it plays alone";
	EXPECT_EQ (rig.dj.playingDeck(), 1);
	EXPECT_EQ (rig.dj.phase(), AutoDj::Phase::playing);
}

TEST (AutoDj, ItGoesOnTrackAfterTrack)
{
	Rig rig;
	rig.dj.setEnabled (true);
	rig.runFor (240.0 * 3);
	EXPECT_GE (rig.loads, 4);
	EXPECT_TRUE (rig.decks[0].playing || rig.decks[1].playing);
}

TEST (AutoDj, WithoutAGridItCrossfadesUnsynced)
{
	Rig rig;
	rig.bpm = 0.0;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	while (! rig.decks[1].playing && rig.decks[0].playing)
		rig.tick();
	EXPECT_FALSE (rig.synced[1]);
	EXPECT_LE (rig.decks[0].length - rig.decks[0].position, AutoDj::noGridMixSeconds + 0.1);
}

TEST (AutoDj, NothingIsLoadedOverALoop)
{
	Rig rig;
	rig.dj.setEnabled (true);
	rig.runFor (0.2);
	rig.decks[1] = { true, false, 20.0, 240.0, 120.0, 0.5, 1.0, true };   // the DJ loops on B
	const auto loads = rig.loads;
	rig.runFor (240.0 - 32.0 - 20.0);
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

TEST (AutoDj, TheFadeIsEqualPower)
{
	EXPECT_DOUBLE_EQ (AutoDj::fadeInDb (0.0), AutoDj::silentDb);
	EXPECT_NEAR (AutoDj::fadeInDb (1.0), 0.0, 1e-9);
	EXPECT_NEAR (AutoDj::fadeInDb (0.5), -3.0103, 1e-3);
	EXPECT_NEAR (AutoDj::fadeOutDb (0.5), -3.0103, 1e-3);
	EXPECT_DOUBLE_EQ (AutoDj::fadeOutDb (1.0), AutoDj::silentDb);
}
