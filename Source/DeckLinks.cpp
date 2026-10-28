// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "DeckLinks.h"

#include "Outputs.h"

namespace
{
	std::string withLetter (const std::string& peer, char letter)
	{
		return juce::String (peer).replace ("{X}", juce::String::charToString (letter)).toStdString();
	}

	void substituteLetter (a3::links::LinkMap& map, char letter)
	{
		for (auto& [port, peers] : map.jack)
			for (auto& peer : peers)
				peer = withLetter (peer, letter);
		for (auto& [from, to] : map.directLinks)
		{
			from = withLetter (from, letter);
			to = withLetter (to, letter);
		}
	}

	std::optional<a3::links::LinkMap> parseFile (const juce::File& file, juce::String& error)
	{
		if (! file.existsAsFile())
		{
			error = "no link map at " + file.getFullPathName();
			return std::nullopt;
		}
		juce::var json;
		if (const auto result = juce::JSON::parse (file.loadFileAsString(), json); result.failed())
		{
			error = file.getFullPathName() + ": " + result.getErrorMessage();
			return std::nullopt;
		}
		auto map = a3::links::parseLinkMap (json, decklinks::ownInputs(), decklinks::ownOutputs(), error);
		if (! map)
			error = file.getFullPathName() + ": " + error;
		return map;
	}
}

std::vector<std::string> decklinks::ownOutputs()
{
	std::vector<std::string> names;
	for (int channel = 0; channel < outputs::oneDeck::channelCount(); ++channel)
		names.push_back (outputs::oneDeck::portName (channel));
	return names;
}

std::vector<std::string> decklinks::ownInputs() { return { "rec_L", "rec_R" }; }

std::optional<a3::links::LinkMap> deckLinkMap (char letter, const std::string& home, const char* envMapFile, juce::String& error)
{
	error = {};
	const auto userFile = juce::File (juce::String (home)).getChildFile (".config/StemDeck/links-" + juce::String::charToString (letter) + ".json");
	if (userFile.existsAsFile())
		return parseFile (userFile, error);

	if (envMapFile == nullptr || *envMapFile == '\0')
		return a3::links::LinkMap {};

	auto map = parseFile (juce::File (juce::String (envMapFile)), error);
	if (map)
		substituteLetter (*map, letter);
	return map;
}
