#include "Session.h"
#include "Buses.h"
#include "MotionLite.h"

#include <cmath>

namespace
{
	double wrapDegrees (double degrees)
	{
		if (! std::isfinite (degrees))
			return 0.0;
		const auto wrapped = std::fmod (degrees + 180.0, 360.0);
		return (wrapped < 0.0 ? wrapped + 360.0 : wrapped) - 180.0;
	}
}

std::unique_ptr<juce::XmlElement> Session::toXml() const
{
	auto xml = std::make_unique<juce::XmlElement> ("StemDeckSession");
	xml->setAttribute ("masterDeck", masterDeck);
	xml->setAttribute ("masterTurnedOff", masterTurnedOff);
	xml->setAttribute ("autoDj", autoDj);

	for (size_t d = 0; d < decks.size(); ++d)
	{
		const auto& deck = decks[d];
		auto* e = xml->createNewChildElement ("Deck");
		e->setAttribute ("index", (int) d);
		e->setAttribute ("set", deck.setId);
		e->setAttribute ("position", deck.position);
		e->setAttribute ("playing", deck.playing);
		e->setAttribute ("cue", deck.cuePoint);
		e->setAttribute ("loopStart", deck.loop.getStart());
		e->setAttribute ("loopEnd", deck.loop.getEnd());
		e->setAttribute ("repeat", deck.repeat);
		e->setAttribute ("tempo", deck.tempo);
		e->setAttribute ("tempoRange", deck.tempoRange);
		e->setAttribute ("vinyl", deck.vinyl);
		e->setAttribute ("sync", deck.sync);
		e->setAttribute ("keyLock", deck.keyLock);
		e->setAttribute ("fader", deck.faderDb);

		for (size_t s = 0; s < deck.stems.size(); ++s)
		{
			auto* stem = e->createNewChildElement ("Stem");
			stem->setAttribute ("gain", deck.stems[s].gainDb);
			stem->setAttribute ("muted", deck.stems[s].muted);
			stem->setAttribute ("buses", (int) deck.stems[s].buses);
			stem->setAttribute ("cue", deck.stems[s].cue);
			stem->setAttribute ("threeD", deck.stems[s].threeD);
			stem->setAttribute ("azimuth", deck.stems[s].azimuth);
			stem->setAttribute ("elevation", deck.stems[s].elevation);
			stem->setAttribute ("width", deck.stems[s].width);
			stem->setAttribute ("lengthBars", deck.stems[s].lengthBars);
		}
	}

	auto* lib = xml->createNewChildElement ("Library");
	lib->setAttribute ("sortColumn", library.sortColumn);
	lib->setAttribute ("sortForwards", library.sortForwards);
	lib->setAttribute ("search", library.search);
	lib->setAttribute ("selected", library.selectedSetId);
	lib->setAttribute ("folder", library.folder);
	lib->setAttribute ("showFolders", library.showFolders);

	auto* jobs = xml->createNewChildElement ("StemJobs");
	for (const auto& job : stemJobs)
	{
		auto* e = jobs->createNewChildElement ("Job");
		e->setAttribute ("input", job.input);
		e->setAttribute ("folder", job.folder);
		e->setAttribute ("track", job.track);
	}

	return xml;
}

Session Session::fromXml (const juce::XmlElement& xml)
{
	Session session;
	session.masterDeck = xml.getIntAttribute ("masterDeck", -1);
	session.masterTurnedOff = xml.getBoolAttribute ("masterTurnedOff");
	session.autoDj = xml.getBoolAttribute ("autoDj");

	for (auto* e : xml.getChildWithTagNameIterator ("Deck"))
	{
		const auto d = e->getIntAttribute ("index", -1);
		if (d < 0 || d >= (int) session.decks.size())
			continue;

		auto& deck = session.decks[(size_t) d];
		deck.setId = e->getStringAttribute ("set");
		deck.position = e->getDoubleAttribute ("position");
		deck.playing = e->getBoolAttribute ("playing");
		deck.cuePoint = e->getDoubleAttribute ("cue");
		deck.loop = { e->getDoubleAttribute ("loopStart"), e->getDoubleAttribute ("loopEnd") };
		deck.repeat = e->getBoolAttribute ("repeat");
		deck.tempo = e->getDoubleAttribute ("tempo", 1.0);
		deck.tempoRange = e->getDoubleAttribute ("tempoRange", 0.08);
		deck.vinyl = e->getBoolAttribute ("vinyl", true);
		deck.sync = e->getBoolAttribute ("sync");
		deck.keyLock = e->getBoolAttribute ("keyLock");
		deck.faderDb = e->getDoubleAttribute ("fader");

		size_t s = 0;
		for (auto* stem : e->getChildWithTagNameIterator ("Stem"))
		{
			if (s >= deck.stems.size())
				break;
			deck.stems[s].gainDb = stem->getDoubleAttribute ("gain");
			deck.stems[s].muted = stem->getBoolAttribute ("muted");
			// An older session's "phones" and CUE bus bit are not read back: the
			// cue send is its own attribute, absent (off) in a file without it.
			deck.stems[s].cue = stem->getBoolAttribute ("cue");
			// Absent in an older file: the defaults of StemMixState. Stored garbage is
			// pulled into IEM's ranges here, so the engine never sees it.
			const StemMixState defaults;
			deck.stems[s].threeD = juce::jlimit (0.0, 1.0, stem->getDoubleAttribute ("threeD", defaults.threeD));
			deck.stems[s].azimuth = wrapDegrees (stem->getDoubleAttribute ("azimuth", defaults.azimuth));
			deck.stems[s].elevation = juce::jlimit (-90.0, 90.0, stem->getDoubleAttribute ("elevation", defaults.elevation));
			deck.stems[s].width = juce::jlimit (0.0, 360.0, stem->getDoubleAttribute ("width", defaults.width));
			deck.stems[s].lengthBars = motionlite::validLength (stem->getDoubleAttribute ("lengthBars", defaults.lengthBars));
			deck.stems[s].buses = buses::fromStored ((unsigned) stem->getIntAttribute ("buses", (int) buses::defaultMask ((int) s)));
			++s;
		}
	}

	if (auto* lib = xml.getChildByName ("Library"))
	{
		session.library.sortColumn = lib->getIntAttribute ("sortColumn");
		session.library.sortForwards = lib->getBoolAttribute ("sortForwards", true);
		session.library.search = lib->getStringAttribute ("search");
		session.library.selectedSetId = lib->getStringAttribute ("selected");
		session.library.folder = lib->getStringAttribute ("folder");
		session.library.showFolders = lib->getBoolAttribute ("showFolders", true);
	}

	if (auto* jobs = xml.getChildByName ("StemJobs"))
		for (auto* e : jobs->getChildWithTagNameIterator ("Job"))
			session.stemJobs.push_back ({ e->getStringAttribute ("input"), e->getStringAttribute ("folder"), e->getStringAttribute ("track") });

	return session;
}

std::optional<Session> Session::load (const juce::File& file)
{
	if (auto xml = juce::XmlDocument::parse (file); xml != nullptr && xml->hasTagName ("StemDeckSession"))
		return fromXml (*xml);
	return std::nullopt;
}
