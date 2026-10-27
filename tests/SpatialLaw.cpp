// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "SpatialLaw.h"
#include "a3-engine/Laws.h"

#include <cmath>
#include <limits>

TEST (SpatialLaw, MovingGainFollowsCoresLaw)
{
    EXPECT_FLOAT_EQ (spatial::movingGain (1.0f), 1.0f);
    const double expected = std::pow (10.0, (a3::laws::bandGain (0.5) * 80.0 - 40.0) / 20.0);
    EXPECT_NEAR (spatial::movingGain (0.5f), expected, 1e-6);
    EXPECT_NEAR (spatial::movingGain (0.001f), 0.01f, 1e-6);
    EXPECT_FLOAT_EQ (spatial::movingGain (2.0f), 1.0f);
}

TEST (SpatialLaw, MovingGainIsExactSilenceAtZero)
{
    EXPECT_EQ (spatial::movingGain (0.0f), 0.0f);
    EXPECT_EQ (spatial::movingGain (1e-10f), 0.0f);
    EXPECT_EQ (spatial::movingGain (-1.0f), 0.0f);
    EXPECT_EQ (spatial::movingGain (std::numeric_limits<float>::quiet_NaN()), 0.0f);
}

TEST (SpatialLaw, EffectiveThreeDMultipliesTheDeck)
{
    EXPECT_FLOAT_EQ (spatial::effectiveThreeD (0.5f, 0.8f, 0.0f), 0.4f);
    EXPECT_EQ (spatial::effectiveThreeD (0.0f, 0.8f, 1.0f), 0.0f);
    EXPECT_FLOAT_EQ (spatial::effectiveThreeD (0.5f, 0.2f, 1.0f), 0.5f);
}

TEST (SpatialLaw, PhonesWeightsAreEqualPower)
{
    const auto w = spatial::phonesWeights (0.8f, 0.5f);
    const float level2 = 0.8f * 0.8f;
    EXPECT_NEAR (w.cue * w.cue + w.mix * w.mix, level2 * level2, 1e-6);
}

TEST (SpatialLaw, PhonesWeightsEndsAreExactSilence)
{
    const auto cueEnd = spatial::phonesWeights (0.5f, 0.0f);
    EXPECT_EQ (cueEnd.mix, 0.0f);
    EXPECT_FLOAT_EQ (cueEnd.cue, 0.25f);
    const auto mixEnd = spatial::phonesWeights (0.5f, 1.0f);
    EXPECT_EQ (mixEnd.cue, 0.0f);
    EXPECT_FLOAT_EQ (mixEnd.mix, 0.25f);
    for (float mix : { 0.0f, 0.3f, 1.0f })
    {
        const auto silent = spatial::phonesWeights (0.0f, mix);
        EXPECT_EQ (silent.cue, 0.0f);
        EXPECT_EQ (silent.mix, 0.0f);
    }
}

TEST (SpatialLaw, TheBedIsReapersSteadyBed)
{
    EXPECT_EQ (spatial::bedSources[0].azimuth, 45.0f);
    EXPECT_EQ (spatial::bedSources[0].side, 0);
    EXPECT_EQ (spatial::bedSources[1].azimuth, -45.0f);
    EXPECT_EQ (spatial::bedSources[1].side, 1);
    EXPECT_EQ (spatial::bedSources[2].azimuth, -135.0f);
    EXPECT_EQ (spatial::bedSources[2].side, 1);
    EXPECT_EQ (spatial::bedSources[3].azimuth, 135.0f);
    EXPECT_EQ (spatial::bedSources[3].side, 0);
    EXPECT_EQ (spatial::bedSourceGainDb, -6.0f);
    EXPECT_EQ (spatial::order, 3);
    EXPECT_EQ (spatial::bformatChannels, 16);
    EXPECT_EQ (spatial::stemsPerDeck, 4);
    EXPECT_EQ (spatial::trim, 0.5f);
}

TEST (SpatialLaw, PhonesDecoderRunsOnlyWhereR12Says)
{
    using spatial::PhonesMode;
    for (bool connected : { false, true })
        for (bool core : { false, true })
        {
            EXPECT_EQ (spatial::phonesDecoderRuns (PhonesMode::on, connected, core), connected);
            EXPECT_FALSE (spatial::phonesDecoderRuns (PhonesMode::off, connected, core));
            EXPECT_EQ (spatial::phonesDecoderRuns (PhonesMode::automatic, connected, core), connected && ! core);
        }
}

// The PhonesMode setting as stored with the app settings; a value nobody
// knows (a hand edit, an older file) is the default.
TEST (SpatialLaw, PhonesModeIsStoredByName)
{
    using spatial::PhonesMode;
    for (auto mode : { PhonesMode::automatic, PhonesMode::on, PhonesMode::off })
        EXPECT_EQ (spatial::phonesModeFrom (spatial::phonesModeName (mode)), mode);
    EXPECT_EQ (spatial::phonesModeName (PhonesMode::automatic), "automatic");
    EXPECT_EQ (spatial::phonesModeFrom (""), PhonesMode::automatic);
    EXPECT_EQ (spatial::phonesModeFrom ("ON"), PhonesMode::automatic);
}

TEST (SpatialLaw, AuxGainIsLinearAndClamped)
{
    EXPECT_FLOAT_EQ (spatial::auxGain (0.37f), 0.37f);
    EXPECT_EQ (spatial::auxGain (1.0f), 1.0f);
    EXPECT_EQ (spatial::auxGain (2.0f), 1.0f);
}

TEST (SpatialLaw, AuxGainIsExactSilenceAtZero)
{
    EXPECT_EQ (spatial::auxGain (0.0f), 0.0f);
    EXPECT_EQ (spatial::auxGain (1e-10f), 0.0f);
    EXPECT_EQ (spatial::auxGain (-1.0f), 0.0f);
    EXPECT_EQ (spatial::auxGain (std::numeric_limits<float>::quiet_NaN()), 0.0f);
}
