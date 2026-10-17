#include <gtest/gtest.h>

#include "RemotePointer.h"

#include <cmath>

using remote::Inject;
using remote::PointerEvent;
using remote::RemotePointerGate;
using Phase = PointerEvent::Phase;

namespace
{
	PointerEvent at (Phase phase, float x, float y) { return { phase, x, y }; }
}

TEST (RemotePointer, ParseRefusesOutOfRange)
{
	EXPECT_FALSE (remote::parsePointer (3, .5f, .5f).has_value());
	EXPECT_FALSE (remote::parsePointer (-1, .5f, .5f).has_value());
	EXPECT_FALSE (remote::parsePointer (0, 1.5f, .5f).has_value());
	EXPECT_FALSE (remote::parsePointer (0, -.1f, .5f).has_value());
	EXPECT_FALSE (remote::parsePointer (0, NAN, .5f).has_value());
	EXPECT_FALSE (remote::parsePointer (0, .5f, INFINITY).has_value());
	const auto edge = remote::parsePointer (0, 0.f, 1.f);
	ASSERT_TRUE (edge.has_value());
	EXPECT_EQ (edge->phase, Phase::down);
	EXPECT_EQ (edge->x, 0.f);
	EXPECT_EQ (edge->y, 1.f);
	EXPECT_EQ (remote::parsePointer (1, .2f, .3f)->phase, Phase::move);
	EXPECT_EQ (remote::parsePointer (2, .2f, .3f)->phase, Phase::up);
}

TEST (RemotePointer, TheRemoteFingerIsNotIndexZero)
{
	EXPECT_EQ (remote::remoteTouchIndex, 9);
}

TEST (RemotePointerGate, NothingWhileNotLive)
{
	RemotePointerGate gate;
	EXPECT_TRUE (gate.pointer (at (Phase::down, .5f, .5f), 0).empty());
	EXPECT_FALSE (gate.held());
	EXPECT_FALSE (gate.isLive (0));
}

TEST (RemotePointerGate, DownMoveUpPassWhileLive)
{
	RemotePointerGate gate;
	gate.live (true, 0);
	const auto down = gate.pointer (at (Phase::down, .1f, .2f), 1);
	ASSERT_EQ (down.size(), 1u);
	EXPECT_EQ (down[0].phase, Phase::down);
	EXPECT_EQ (down[0].x, .1f);
	EXPECT_EQ (down[0].y, .2f);
	const auto move = gate.pointer (at (Phase::move, .3f, .4f), 2);
	ASSERT_EQ (move.size(), 1u);
	EXPECT_EQ (move[0].phase, Phase::move);
	EXPECT_EQ (move[0].x, .3f);
	const auto up = gate.pointer (at (Phase::up, .5f, .6f), 3);
	ASSERT_EQ (up.size(), 1u);
	EXPECT_EQ (up[0].phase, Phase::up);
	EXPECT_EQ (up[0].y, .6f);
	EXPECT_FALSE (gate.held());
}

TEST (RemotePointerGate, GoingOffMidDragReleases)
{
	RemotePointerGate gate;
	gate.live (true, 0);
	gate.pointer (at (Phase::down, .2f, .3f), 1);
	const auto out = gate.live (false, 10);
	ASSERT_EQ (out.size(), 1u);
	EXPECT_EQ (out[0].phase, Phase::up);
	EXPECT_EQ (out[0].x, .2f);
	EXPECT_EQ (out[0].y, .3f);
	EXPECT_FALSE (gate.held());
	EXPECT_FALSE (gate.isLive (10));
	EXPECT_TRUE (gate.pointer (at (Phase::move, .2f, .3f), 11).empty());
}

TEST (RemotePointerGate, GoingOffWhileNotHeldReleasesNothing)
{
	RemotePointerGate gate;
	gate.live (true, 0);
	EXPECT_TRUE (gate.live (false, 10).empty());
}

TEST (RemotePointerGate, ASilentHeldPointerIsReleasedAfterOneSecond)
{
	RemotePointerGate gate;
	gate.live (true, 0);
	gate.pointer (at (Phase::down, .2f, .3f), 0);
	EXPECT_TRUE (gate.tick (999).empty());
	const auto out = gate.tick (1001);
	ASSERT_EQ (out.size(), 1u);
	EXPECT_EQ (out[0].phase, Phase::up);
	EXPECT_FALSE (gate.held());
	EXPECT_TRUE (gate.tick (1002).empty());
}

TEST (RemotePointerGate, AKeepAliveMoveHoldsThePointer)
{
	RemotePointerGate gate;
	gate.live (true, 0);
	gate.pointer (at (Phase::down, .2f, .3f), 0);
	gate.pointer (at (Phase::move, .2f, .3f), 800);
	EXPECT_TRUE (gate.tick (1500).empty());
	EXPECT_TRUE (gate.held());
}

TEST (RemotePointerGate, TheLeaseRunsOut)
{
	RemotePointerGate gate;
	gate.live (true, 0);
	EXPECT_TRUE (gate.isLive (11999));
	EXPECT_FALSE (gate.isLive (12001));

	RemotePointerGate held;
	held.live (true, 0);
	held.pointer (at (Phase::down, .2f, .3f), 0);
	const auto out = held.tick (12001);
	ASSERT_EQ (out.size(), 1u);
	EXPECT_EQ (out[0].phase, Phase::up);
	EXPECT_FALSE (held.isLive (12001));
}

TEST (RemotePointerGate, ARepeatedOnRenewsTheLease)
{
	RemotePointerGate gate;
	gate.live (true, 0);
	gate.live (true, 5000);
	EXPECT_TRUE (gate.isLive (16999));
	EXPECT_FALSE (gate.isLive (17001));
}

TEST (RemotePointerGate, AMoveWithoutDownIsDropped)
{
	RemotePointerGate gate;
	gate.live (true, 0);
	EXPECT_TRUE (gate.pointer (at (Phase::move, .5f, .5f), 1).empty());
	EXPECT_TRUE (gate.pointer (at (Phase::up, .5f, .5f), 2).empty());
}

TEST (RemotePointerGate, ASecondDownReleasesTheFirst)
{
	RemotePointerGate gate;
	gate.live (true, 0);
	gate.pointer (at (Phase::down, .1f, .1f), 1);
	const auto out = gate.pointer (at (Phase::down, .7f, .8f), 2);
	ASSERT_EQ (out.size(), 2u);
	EXPECT_EQ (out[0].phase, Phase::up);
	EXPECT_EQ (out[0].x, .1f);
	EXPECT_EQ (out[1].phase, Phase::down);
	EXPECT_EQ (out[1].x, .7f);
	EXPECT_TRUE (gate.held());
}
