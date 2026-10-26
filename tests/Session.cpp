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

TEST (Session, SpatialStateRoundTrips)
{
	Session session;
	auto& stem = session.decks[1].stems[2];
	stem.threeD = 0.75;
	stem.azimuth = -120.5;
	stem.elevation = 33.0;
	stem.width = 200.0;

	const auto back = throughXml (session).decks[1].stems[2];
	EXPECT_DOUBLE_EQ (back.threeD, 0.75);
	EXPECT_DOUBLE_EQ (back.azimuth, -120.5);
	EXPECT_DOUBLE_EQ (back.elevation, 33.0);
	EXPECT_DOUBLE_EQ (back.width, 200.0);
}

TEST (Session, AnOlderSessionHasDefaultSpatialState)
{
	auto xml = Session().toXml();
	for (auto* deck : xml->getChildWithTagNameIterator ("Deck"))
		for (auto* stem : deck->getChildWithTagNameIterator ("Stem"))
			for (const auto* name : { "threeD", "azimuth", "elevation", "width" })
				stem->removeAttribute (name);

	const auto back = Session::fromXml (*xml);
	for (const auto& deck : back.decks)
		for (const auto& stem : deck.stems)
		{
			EXPECT_DOUBLE_EQ (stem.threeD, 0.0);
			EXPECT_DOUBLE_EQ (stem.azimuth, 0.0);
			EXPECT_DOUBLE_EQ (stem.elevation, 0.0);
			EXPECT_DOUBLE_EQ (stem.width, 60.0);
		}
}

TEST (Session, GarbageSpatialValuesAreClamped)
{
	auto xml = Session().toXml();
	auto* deck = xml->getChildByName ("Deck");
	auto* first = deck->getChildByName ("Stem");
	first->setAttribute ("threeD", 7.0);
	first->setAttribute ("azimuth", 400.0);
	first->setAttribute ("elevation", 120.0);
	first->setAttribute ("width", -5.0);
	auto* second = first->getNextElementWithTagName ("Stem");
	second->setAttribute ("threeD", -2.0);
	second->setAttribute ("elevation", -120.0);
	second->setAttribute ("width", 400.0);

	const auto back = Session::fromXml (*xml).decks[0];
	EXPECT_DOUBLE_EQ (back.stems[0].threeD, 1.0);
	EXPECT_DOUBLE_EQ (back.stems[0].azimuth, 40.0);
	EXPECT_DOUBLE_EQ (back.stems[0].elevation, 90.0);
	EXPECT_DOUBLE_EQ (back.stems[0].width, 0.0);
	EXPECT_DOUBLE_EQ (back.stems[1].threeD, 0.0);
	EXPECT_DOUBLE_EQ (back.stems[1].elevation, -90.0);
	EXPECT_DOUBLE_EQ (back.stems[1].width, 360.0);
}

TEST (Session, LengthRoundTripsAndOldSessionsHaveFour)
{
	Session session;
	session.decks[1].stems[2].lengthBars = 0.5;
	session.decks[0].stems[0].lengthBars = 8.0;
	const auto back = throughXml (session);
	EXPECT_DOUBLE_EQ (back.decks[1].stems[2].lengthBars, 0.5);
	EXPECT_DOUBLE_EQ (back.decks[0].stems[0].lengthBars, 8.0);
	EXPECT_DOUBLE_EQ (back.decks[0].stems[1].lengthBars, 4.0);

	auto xml = Session().toXml();
	for (auto* deck : xml->getChildIterator())
		if (deck->hasTagName ("Deck"))
			for (auto* stem : deck->getChildWithTagNameIterator ("Stem"))
				stem->removeAttribute ("lengthBars");
	for (const auto& deck : Session::fromXml (*xml).decks)
		for (const auto& stem : deck.stems)
			EXPECT_DOUBLE_EQ (stem.lengthBars, 4.0);
}

TEST (Session, AStoredLengthOutsideTheValidSetReadsAsFour)
{
	auto xml = Session().toXml();
	auto* first = xml->getChildByName ("Deck")->getChildByName ("Stem");
	first->setAttribute ("lengthBars", 3.0);
	auto* second = first->getNextElementWithTagName ("Stem");
	second->setAttribute ("lengthBars", "garbage");
	auto* third = second->getNextElementWithTagName ("Stem");
	third->setAttribute ("lengthBars", 16.0);
	auto* fourth = third->getNextElementWithTagName ("Stem");
	fourth->setAttribute ("lengthBars", 2.0);

	const auto back = Session::fromXml (*xml).decks[0];
	EXPECT_DOUBLE_EQ (back.stems[0].lengthBars, 4.0);
	EXPECT_DOUBLE_EQ (back.stems[1].lengthBars, 4.0);
	EXPECT_DOUBLE_EQ (back.stems[2].lengthBars, 4.0);
	EXPECT_DOUBLE_EQ (back.stems[3].lengthBars, 2.0);
}
