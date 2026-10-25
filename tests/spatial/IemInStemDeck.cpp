// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <IemNode.h>

namespace
{
std::unique_ptr<a3::IemNode> makeEncoder()
{
    return a3::IemNode::create (a3::IemKind::multiEncoder, 4, 16);
}
}

TEST (IemInStemDeck, AMultiEncoderBuildsAndRuns)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    auto node = makeEncoder();
    ASSERT_TRUE (node != nullptr);
    node->prepare (44100.0, 128);
    ASSERT_TRUE (node->setParameter ("azimuth0", 0.0f).wasOk());

    juce::AudioBuffer<float> io (16, 128);
    float energyInW = 0.0f;
    for (int block = 0; block < 4; ++block)
    {
        io.clear();
        for (int i = 0; i < 128; ++i)
            io.setSample (0, i, 0.5f);
        node->process (io);
        energyInW += io.getMagnitude (0, 0, 128);
    }
    EXPECT_GT (energyInW, 0.0f);
}

TEST (IemInStemDeck, ItsOscReceiverIsClosed)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    auto node = makeEncoder();
    ASSERT_TRUE (node != nullptr);
    EXPECT_FALSE (node->listensForOsc());
}
