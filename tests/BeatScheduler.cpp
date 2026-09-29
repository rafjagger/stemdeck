#include <gtest/gtest.h>

#include "BeatScheduler.h"

#include <vector>

namespace
{
	struct Sent { double at; int beatInBar; };

	// The sender thread against a perfect clock: the deck is where the clock
	// says, every sleep overruns by `overrun`, and each round costs `work`.
	std::vector<Sent> run (BeatScheduler& scheduler, double from, double to, double bpm,
						   double overrun = 0.0003, double work = 0.0001)
	{
		std::vector<Sent> sent;
		double t = from;
		while (t < to)
		{
			const auto s = scheduler.step (t, t, 1.0, 0.0, bpm, true);
			t += s.sleepBefore + overrun;
			if (s.send)
				sent.push_back ({ t, s.send->beatInBar });
			t += work;
		}
		return sent;
	}
}

TEST (BeatScheduler, NoBeatIsLostOverThirtySeconds)
{
	BeatScheduler scheduler;
	const auto sent = run (scheduler, 0.05, 30.0, 128.0);
	EXPECT_EQ (sent.size(), 64u) << "every beat in (0, 30] s at 128 BPM";
	const auto beat = 60.0 / 128.0;
	for (size_t i = 0; i < sent.size(); ++i)
		EXPECT_NEAR (sent[i].at, (double) (i + 1) * beat, 0.002) << "beat " << i + 1;
}

TEST (BeatScheduler, EvenWithLongOverrunsNoBeatIsLost)
{
	BeatScheduler scheduler;
	const auto sent = run (scheduler, 0.05, 30.0, 128.0, 0.004, 0.001);
	EXPECT_EQ (sent.size(), 64u);
}

TEST (BeatScheduler, TheBarIsCounted)
{
	BeatScheduler scheduler;
	const auto sent = run (scheduler, 0.05, 2.1, 120.0); // beats at 0.5 .. 2.0
	ASSERT_EQ (sent.size(), 4u);
	EXPECT_EQ (sent[0].beatInBar, 2);
	EXPECT_EQ (sent[3].beatInBar, 1);
}

TEST (BeatScheduler, StartingOnABeatSendsThatBeat)
{
	BeatScheduler scheduler;
	const auto s = scheduler.step (10.0, 1.5, 1.0, 0.0, 120.0, true); // the 4th beat, a cue
	ASSERT_TRUE (s.send.has_value());
	EXPECT_DOUBLE_EQ (s.sleepBefore, 0.0);
	EXPECT_NEAR (s.send->trackSeconds, 1.5, 1e-9);
	EXPECT_EQ (s.send->beatInBar, 4);
}

TEST (BeatScheduler, AJumpBackIsNotABurst)
{
	BeatScheduler scheduler;
	scheduler.step (10.000, 3.900, 1.0, 0.0, 120.0, true);
	const auto s = scheduler.step (10.006, 1.200, 1.0, 0.0, 120.0, true); // cue back
	EXPECT_FALSE (s.send.has_value() && s.sleepBefore == 0.0) << "nothing sent for the jump itself";
}

TEST (BeatScheduler, AHandoverDoesNotDoubleABeat)
{
	BeatScheduler scheduler;
	// Deck A's beat at 1.0 s goes out ...
	const auto a = scheduler.step (100.0, 0.999, 1.0, 0.0, 60.0, true);
	ASSERT_TRUE (a.send.has_value());
	// ... and 3 ms later deck B is master, its grid 3 ms behind A's.
	const auto b = scheduler.step (100.002, 0.998, 1.0, 0.0, 60.0, true);
	EXPECT_FALSE (b.send.has_value()) << "one beat, not two, 3 ms apart";
}

TEST (BeatScheduler, AStoppedDeckSendsNothing)
{
	BeatScheduler scheduler;
	EXPECT_FALSE (scheduler.step (10.0, 1.499, 1.0, 0.0, 120.0, false).send.has_value());
}
