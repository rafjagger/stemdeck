// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include "AppSettings.h"
#include "AudioOutput.h"
#include "CoreTruth.h"
#include "DeckInstance.h"
#include "OneDeckAudio.h"
#include "RecorderControls.h"
#include "Session.h"
#include "StemCreatorLink.h"
#include "StemLibrary.h"
#include "Theme.h"

// The content of a -bformat instance (ruling R10): one deck, its JACK client
// StemDeck-<X> with the 22 one-deck ports and rec_L/R, its own session
// (~/.config/StemDeck/<X>/session.xml, imported once from the two-deck one),
// the library, the stem creator and the recorder as the two-deck screen has
// them. No buses, no Auto DJ (B15), no panel, no remote link yet.
//
// The screen is a placeholder until the v4 screen (S4.4b): the deck letter,
// the loaded set, Load and PLAY above the library.
class OneDeckComponent : public juce::Component,
						 private juce::Timer
{
public:
	explicit OneDeckComponent (DeckInstance);
	~OneDeckComponent() override;

	juce::PropertiesFile& settings() { return *appProperties.getUserSettings(); }

	void paint (juce::Graphics&) override;
	void resized() override;

private:
	void loadSet (const StemSet&);
	void togglePlay();
	void updatePhonesDecoder();
	void restoreSession();
	void saveSession();
	OneDeckSession gatherSession() const;
	void updateDeviceStatus();
	void showSettings();
	void timerCallback() override;

	const DeckInstance instance;
	DJLookAndFeel lookAndFeel;
	juce::ApplicationProperties appProperties;
	coretruth::Chosen truth;
	std::unique_ptr<TruthKeeperLink> truthKeeper;   // also says whether Core is heard (R12)

	juce::AudioFormatManager formatManager;
	juce::ThreadPool analysisPool { juce::ThreadPoolOptions{}.withThreadName ("Tempo analysis").withNumberOfThreads (2) };
	AnalysisCache analysisCache { appsettings::analysisCacheFile() };
	OneDeckAudio deckAudio { formatManager, analysisPool, analysisCache };
	juce::String spatialError;
	spatial::PhonesMode phonesMode = spatial::PhonesMode::automatic;

	StemLibrary library { formatManager };
	StemCreator stemCreator;
	std::unique_ptr<StemCreatorLink> creatorLink;
	Recorder recorder;
	RecorderControls rec { recorder };
	std::unique_ptr<AudioOutput> audio;

	// What the placeholder does not show (tempo, stem knobs, mutes, cues),
	// kept from the restore so a save does not lose it.
	OneDeckSession restored;
	juce::String lastSessionText;
	int sessionCountdown = 0, statusCountdown = 0;

	juce::Label deckLabel, setLabel, deviceStatus;
	juce::TextButton loadButton { "Load" }, playButton { "PLAY" }, settingsButton { "Settings" };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OneDeckComponent)
};
