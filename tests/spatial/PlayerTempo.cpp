// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "PlayerTempo.h"
#include "TwoDeckFixture.h"

// The tempo a deck reports to Core is its set tempo: the grid times the tempo
// fader. The jog bend and the sync nudge are momentary phase corrections; a
// delay retuned on each of them would be heard.
namespace
{
class PlayerTempo : public ::testing::Test
{
protected:
	void SetUp() override
	{
		ASSERT_TRUE (folder.createDirectory().wasOk());
		formats.registerBasicFormats();
		StemSet set;
		set.name = "noise";
		for (int s = 0; s < twodeck::stems; ++s)
		{
			set.files[(size_t) s] = folder.getChildFile ("stem" + juce::String (s) + ".wav");
			twodeck::writeNoiseStem (set.files[(size_t) s], 31 + s);
		}
		ASSERT_EQ (player.load (set), juce::String());
		player.setBeatGrid ({ 120.0, 0.0 });
	}

	void TearDown() override { folder.deleteRecursively(); }

	double reported() const { return tempoOfPlayer (player, false).bpm; }

	juce::ScopedJuceInitialiser_GUI juce;
	juce::File folder { juce::File::createTempFile ("player-tempo") };
	juce::AudioFormatManager formats;
	StemDeckPlayer player { formats };
};
}

TEST_F (PlayerTempo, TheTempoFaderMovesTheReport)
{
	player.setSpeed (1.05);
	EXPECT_DOUBLE_EQ (reported(), 126.0);
}

TEST_F (PlayerTempo, AJogBendLeavesTheReportAlone)
{
	player.setSpeed (1.05);
	player.setPitchBend (1.4);
	EXPECT_DOUBLE_EQ (reported(), 126.0);
}

TEST_F (PlayerTempo, ASyncNudgeLeavesTheReportAlone)
{
	player.setSpeed (1.05);
	player.setSyncNudge (0.98);
	EXPECT_DOUBLE_EQ (reported(), 126.0);
}
