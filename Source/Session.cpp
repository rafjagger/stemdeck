#include "Session.h"

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
		e->setAttribute ("fader", deck.faderDb);
		e->setAttribute ("phones", deck.phones);

		for (size_t s = 0; s < deck.stems.size(); ++s)
		{
			auto* stem = e->createNewChildElement ("Stem");
			stem->setAttribute ("gain", deck.stems[s].gainDb);
			stem->setAttribute ("muted", deck.stems[s].muted);
			stem->setAttribute ("buses", (int) deck.stems[s].buses);
		}
	}

	auto* lib = xml->createNewChildElement ("Library");
	lib->setAttribute ("sortColumn", library.sortColumn);
	lib->setAttribute ("sortForwards", library.sortForwards);
	lib->setAttribute ("search", library.search);
	lib->setAttribute ("selected", library.selectedSetId);

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
		deck.faderDb = e->getDoubleAttribute ("fader");
		deck.phones = e->getBoolAttribute ("phones");

		size_t s = 0;
		for (auto* stem : e->getChildWithTagNameIterator ("Stem"))
		{
			if (s >= deck.stems.size())
				break;
			deck.stems[s].gainDb = stem->getDoubleAttribute ("gain");
			deck.stems[s].muted = stem->getBoolAttribute ("muted");
			deck.stems[s].buses = (unsigned) stem->getIntAttribute ("buses", 1 << (int) s);
			++s;
		}
	}

	if (auto* lib = xml.getChildByName ("Library"))
	{
		session.library.sortColumn = lib->getIntAttribute ("sortColumn");
		session.library.sortForwards = lib->getBoolAttribute ("sortForwards", true);
		session.library.search = lib->getStringAttribute ("search");
		session.library.selectedSetId = lib->getStringAttribute ("selected");
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
