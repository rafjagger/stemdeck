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
