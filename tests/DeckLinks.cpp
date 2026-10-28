// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "DeckLinks.h"

#include <a3-links/LinkPlan.h>

namespace
{
	using Peers = std::vector<std::string>;

	// A home of its own per test: the user file lives under it.
	struct Home
	{
		explicit Home (const char* name)
			: dir (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sd-decklinks-" + juce::String (name)))
		{
			dir.deleteRecursively();
			dir.createDirectory();
		}
		~Home() { dir.deleteRecursively(); }

		std::string path() const { return dir.getFullPathName().toStdString(); }
		juce::File userFile (char letter) const
		{
			return dir.getChildFile (".config/StemDeck/links-" + juce::String::charToString (letter) + ".json");
		}
		juce::File write (const juce::String& relative, const juce::String& text) const
		{
			const auto file = dir.getChildFile (relative);
			file.getParentDirectory().createDirectory();
			file.replaceWithText (text);
			return file;
		}

		juce::File dir;
	};

	juce::String userText (const juce::String& peer) { return R"({"jack": {"bformat_1": [")" + peer + R"("]}})"; }
}

TEST (DeckLinks, UserFileWins)
{
	const Home home ("user");
	home.write (".config/StemDeck/links-B.json", userText ("user:in_1"));
	const auto envFile = home.write ("env.json", userText ("env:in_1"));
	juce::String error;

	const auto map = deckLinkMap ('B', home.path(), envFile.getFullPathName().toRawUTF8(), error);

	ASSERT_TRUE (map.has_value()) << error;
	EXPECT_EQ (map->jack.at ("bformat_1"), Peers { "user:in_1" });
}

TEST (DeckLinks, EnvFileWithLetterSubstituted)
{
	const Home home ("env");
	const auto envFile = home.write ("env.json",
		R"({"jack": {"bformat_1": ["AES67 deck {X}:send_MAIN1"], "rec_L": ["deck {X} from {X}:out"]}})");
	juce::String error;

	const auto map = deckLinkMap ('B', home.path(), envFile.getFullPathName().toRawUTF8(), error);

	ASSERT_TRUE (map.has_value()) << error;
	EXPECT_EQ (map->jack.at ("bformat_1"), Peers { "AES67 deck B:send_MAIN1" });
	EXPECT_EQ (map->jack.at ("rec_L"), Peers { "deck B from B:out" });
}

TEST (DeckLinks, NoFileIsEmpty)
{
	const Home home ("none");
	juce::String error;

	const auto map = deckLinkMap ('A', home.path(), nullptr, error);

	ASSERT_TRUE (map.has_value()) << error;
	EXPECT_TRUE (map->jack.empty());
	EXPECT_TRUE (map->directLinks.empty());
	EXPECT_TRUE (error.isEmpty());
}

TEST (DeckLinks, NamedEnvFileThatIsMissingIsRefused)
{
	const Home home ("missing");
	juce::String error;

	const auto map = deckLinkMap ('B', home.path(), (home.path() + "/nowhere.json").c_str(), error);

	EXPECT_FALSE (map.has_value());
	EXPECT_TRUE (error.contains ("nowhere.json")) << error;
}

TEST (DeckLinks, UnknownOwnPortIsRefused)
{
	const Home home ("unknown");
	home.write (".config/StemDeck/links-B.json", R"({"jack": {"cue_10": ["x:y"]}})");
	juce::String error;

	const auto map = deckLinkMap ('B', home.path(), nullptr, error);

	EXPECT_FALSE (map.has_value());
	EXPECT_TRUE (error.contains ("cue_10")) << error;
}

TEST (DeckLinks, ShippedAes67MapHas43Links)
{
	const Home home ("shipped");
	juce::String error;

	const auto map = deckLinkMap ('B', home.path(), STEMDECK_DATA_DIR "/links-aes67.json", error);

	ASSERT_TRUE (map.has_value()) << error;
	const auto inputs = decklinks::ownInputs();
	EXPECT_EQ (inputs, Peers ({ "rec_L", "rec_R" }));
	EXPECT_EQ (decklinks::ownOutputs().size(), 43u);

	std::size_t outgoing = 0, incoming = 0;
	for (const auto& [port, peers] : map->jack)
	{
		EXPECT_EQ (peers.size(), 1u) << port;
		(a3::links::isInputPort (port, inputs) ? incoming : outgoing) += peers.size();
	}
	EXPECT_EQ (outgoing, 41u);
	EXPECT_EQ (incoming, 2u);
	EXPECT_TRUE (map->directLinks.empty());

	EXPECT_EQ (map->jack.at ("bformat_1"), Peers { "AES67 deck B:send_MAIN1" });
	EXPECT_EQ (map->jack.at ("bformat_16"), Peers { "AES67 deck B:send_MAIN16" });
	EXPECT_EQ (map->jack.at ("aux_1"), Peers { "AES67 deck B:send_AUX1" });
	EXPECT_EQ (map->jack.at ("aux_16"), Peers { "AES67 deck B:send_AUX16" });
	EXPECT_EQ (map->jack.at ("cue_1"), Peers { "AES67 deck B:send_CUE1" });
	EXPECT_EQ (map->jack.at ("cue_9"), Peers { "AES67 deck B:send_CUE9" });
	EXPECT_EQ (map->jack.at ("rec_L"), Peers { "AES67 a3nuc1 out:receive_CH1" });
	EXPECT_EQ (map->jack.at ("rec_R"), Peers { "AES67 a3nuc1 out:receive_CH2" });
	EXPECT_EQ (map->jack.count ("phones_L"), 0u);
	EXPECT_EQ (map->jack.count ("phones_R"), 0u);
}

TEST (DeckLinks, ForeignLinkIsNeverRemoved)
{
	const Home home ("foreign");
	juce::String error;
	const auto map = deckLinkMap ('B', home.path(), STEMDECK_DATA_DIR "/links-aes67.json", error);
	ASSERT_TRUE (map.has_value()) << error;

	const a3::links::Link foreign { "StemDeck-B:bformat_1", "somebody:in_1" };
	const auto plan = a3::links::planConnections (*map, { foreign }, {},
		{ "StemDeck-B:bformat_1", "somebody:in_1", "AES67 deck B:send_MAIN1" }, decklinks::ownInputs(), "StemDeck-B");

	EXPECT_TRUE (plan.remove.empty());
	ASSERT_EQ (plan.add.size(), 1u);
	EXPECT_EQ (plan.add.front(), (a3::links::Link { "StemDeck-B:bformat_1", "AES67 deck B:send_MAIN1" }));
}
