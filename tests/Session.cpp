#include <gtest/gtest.h>

#include "Session.h"

namespace
{
	Session throughXml (const Session& session)
	{
		return Session::fromXml (*session.toXml());
	}
}

TEST (Session, KeyLockIsKeptPerDeck)
{
	Session session;
	session.decks[0].keyLock = true;
	session.decks[1].keyLock = false;

	const auto back = throughXml (session);
	EXPECT_TRUE (back.decks[0].keyLock);
	EXPECT_FALSE (back.decks[1].keyLock);

	session.decks[0].keyLock = false;
	session.decks[1].keyLock = true;
	const auto swapped = throughXml (session);
	EXPECT_FALSE (swapped.decks[0].keyLock);
	EXPECT_TRUE (swapped.decks[1].keyLock);
}

// A session written before key lock existed plays as it did: pitch follows tempo.
TEST (Session, AnOlderSessionHasKeyLockOff)
{
	auto xml = Session().toXml();
	for (auto* deck : xml->getChildWithTagNameIterator ("Deck"))
		deck->removeAttribute ("keyLock");

	for (const auto& deck : Session::fromXml (*xml).decks)
		EXPECT_FALSE (deck.keyLock);
}

TEST (Session, TheDeckControlsRoundTrip)
{
	Session session;
	auto& deck = session.decks[1];
	deck.tempo = 1.04;
	deck.tempoRange = 0.16;
	deck.vinyl = false;
	deck.sync = true;
	deck.repeat = true;

	const auto back = throughXml (session).decks[1];
	EXPECT_DOUBLE_EQ (back.tempo, 1.04);
	EXPECT_DOUBLE_EQ (back.tempoRange, 0.16);
	EXPECT_FALSE (back.vinyl);
	EXPECT_TRUE (back.sync);
	EXPECT_TRUE (back.repeat);
}

TEST (Session, CueIsKeptPerStem)
{
	Session session;
	session.decks[0].stems[1].cue = true;
	session.decks[1].stems[3].cue = true;
	session.decks[1].stems[3].muted = true;

	const auto back = throughXml (session);
	for (int d = 0; d < 2; ++d)
		for (int s = 0; s < 4; ++s)
			EXPECT_EQ (back.decks[(size_t) d].stems[(size_t) s].cue, (d == 0 && s == 1) || (d == 1 && s == 3));
	EXPECT_TRUE (back.decks[1].stems[3].muted);
}

TEST (Session, CueIsIndependentOfTheBuses)
{
	Session session;
	session.decks[0].stems[0].buses = 0b00001;
	session.decks[0].stems[0].cue = true;

	const auto back = throughXml (session).decks[0].stems[0];
	EXPECT_EQ (back.buses, 0b00001u);
	EXPECT_TRUE (back.cue);
}

// A session from the days of the CUE bus has it as bit 5 of the bus mask;
// that stays dropped -- the new cue is a field of its own, off in an older file.
TEST (Session, AnOldCueBusBitDoesNotTurnCueOn)
{
	auto xml = Session().toXml();
	for (auto* deck : xml->getChildWithTagNameIterator ("Deck"))
		for (auto* stem : deck->getChildWithTagNameIterator ("Stem"))
		{
			stem->setAttribute ("buses", 0b110001);
			stem->removeAttribute ("cue");
		}

	for (const auto& deck : Session::fromXml (*xml).decks)
		for (const auto& stem : deck.stems)
		{
			EXPECT_EQ (stem.buses, 0b010001u);
			EXPECT_FALSE (stem.cue);
		}
}
