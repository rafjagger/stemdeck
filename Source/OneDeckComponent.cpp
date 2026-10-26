// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneDeckComponent.h"

#include "AppSettings.h"
#include "OscTruthFile.h"
#include "SettingsPanel.h"
#include "StemKnob.h"
#include "Tips.h"

#include <iostream>

namespace
{
double nowSeconds() { return juce::Time::getMillisecondCounterHiRes() / 1000.0; }
constexpr const char* phonesModeKey = "phonesMode";
}

OneDeckComponent::OneDeckComponent (DeckInstance deck)
	: instance (deck), truth (coretruth::choose())
{
	setLookAndFeel (&lookAndFeel);
	appProperties.setStorageParameters (appsettings::options());
	formatManager.registerBasicFormats();
	phonesMode = spatial::phonesModeFrom (settings().getValue (phonesModeKey).toStdString());

	{
		std::string truthError;
		rec.setMeterParameters (osctruth::readMeterParameters (truth.path, truthError));
		if (! truthError.empty())
			std::cerr << "StemDeck: " << truthError << std::endl;
	}

	auto& host = deckAudio.host();
	host.onAnalysisStored = [this] { library.analysisChanged(); };
	if (const auto error = deckAudio.createSpatial(); error.isNotEmpty())
		spatialError = error;

	deckLabel.setText (juce::String (instance.title()), juce::dontSendNotification);
	deckLabel.setFont (juce::FontOptions (28.0f, juce::Font::bold));
	deckLabel.setColour (juce::Label::textColourId, Theme::deck (instance.letter - 'A'));
	setLabel.setColour (juce::Label::textColourId, Theme::text);
	deviceStatus.setColour (juce::Label::textColourId, Theme::textDim);
	for (auto* label : { &deckLabel, &setLabel, &deviceStatus })
		addAndMakeVisible (label);

	loadButton.setTooltip ("Load the set selected in the library");
	loadButton.onClick = [this] { if (const auto* set = library.selectedSet()) loadSet (*set); };
	playButton.setClickingTogglesState (false);
	playButton.setColour (juce::TextButton::buttonOnColourId, Theme::play);
	playButton.onClick = [this] { togglePlay(); };
	settingsButton.onClick = [this] { showSettings(); };
	for (auto* button : { &loadButton, &playButton, &settingsButton })
	{
		button->setMouseClickGrabsKeyboardFocus (false);
		addAndMakeVisible (button);
	}

	rec.button.onClick = [this] { rec.toggle (audio->jackClient().getSampleRate()); };
	rec.addTo (*this);

	addAndMakeVisible (library);
	// Either of the library's Load keys loads the one deck (one Load in S4.4b).
	library.onLoadSet = [this] (const StemSet& set, int) { loadSet (set); };
	library.lookUpBeatGrid = [this] (const StemSet& set) { return analysisCache.find (set); };
	creatorLink = std::make_unique<StemCreatorLink> (library, stemCreator, settings(), *this);

	juce::StringArray portNames;
	for (int ch = 0; ch < outputs::oneDeck::channelCount(); ++ch)
		portNames.add (juce::String (outputs::oneDeck::portName (ch)));
	audio = std::make_unique<AudioOutput> (settings());
	audio->onDeviceChanged = [this] { updateDeviceStatus(); };
	audio->open (juce::String (instance.jackClientName()), portNames, deckAudio, { "rec_L", "rec_R" }, &recorder);
	rec.button.setEnabled (audio->usingJack());

	truthKeeper = coretruth::follow (truth);

	setSize (768, 1024);
	restoreSession();
	updatePhonesDecoder();
	updateDeviceStatus();
	startTimerHz (30);
}

OneDeckComponent::~OneDeckComponent()
{
	stopTimer();
	saveSession();
	audio->closeJack();   // no more input blocks ...
	recorder.stop();      // ... then the file is closed complete
	analysisPool.removeAllJobs (true, 5000);
	audio->close();
	setLookAndFeel (nullptr);
}

//==============================================================================
void OneDeckComponent::loadSet (const StemSet& set)
{
	if (const auto error = deckAudio.host().load (set); error.isNotEmpty())
	{
		juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Cannot load the set", error);
		return;
	}
	setLabel.setText (set.name, juce::dontSendNotification);
}

void OneDeckComponent::togglePlay()
{
	auto& player = deckAudio.host().player();
	if (! player.isLoaded())
		return;
	if (player.isPlaying())
		player.pause();
	else
		player.play();
}

// R12, every tick: the phones ports heard (under JACK connected, on a device
// with an output to land on) and Core heard within the last five seconds.
void OneDeckComponent::updatePhonesDecoder()
{
	const auto phones = outputs::oneDeck::layout().phones;
	const auto connected = audio->usingJack() ? audio->isPortConnected (phones) || audio->isPortConnected (phones + 1)
											  : outputs::oneDeck::foldsInto (phones, audio->activeDeviceOutputs()) >= 0;
	const auto lastCore = truthKeeper != nullptr ? truthKeeper->lastAnnouncementSeconds() : std::nullopt;
	deckAudio.updatePhonesDecoder (phonesMode, connected, spatial::coreHeard (lastCore, nowSeconds()));
}

//==============================================================================
// The two-deck session is read, never written: two-deck mode still owns it.
// An import is saved at once, so it happens on the first start only.
void OneDeckComponent::restoreSession()
{
	const auto settingsFile = settings().getFile();
	restored = loadOrImport (oneDeckSessionFile (settingsFile, instance), Session::file (settingsFile), instance);
	const auto& deck = restored.deck;
	auto& host = deckAudio.host();
	auto& player = host.player();

	library.restoreState (restored.library);
	host.restoreSpatial (deck);
	for (int s = 0; s < StemDeckPlayer::numStems; ++s)
	{
		const auto& stem = deck.stems[(size_t) s];
		player.setStemGain (s, juce::Decibels::decibelsToGain ((float) stem.gainDb, stemKnobMinimumDb));
		player.setStemMuted (s, stem.muted);
		player.setStemCue (s, stem.cue);
	}

	if (deck.setId.isNotEmpty())
		if (const auto* set = library.findSet (library.idFromStored (deck.setId)))
			loadSet (*set);
	if (player.isLoaded())
	{
		host.restoreTransport (deck);
		if (deck.playing)
			player.play();   // what was playing plays on
	}

	creatorLink->restore (restored.stemJobs);

	if (restored.imported)
		saveSession();
	else
		lastSessionText = restored.toXml()->toString();
}

OneDeckSession OneDeckComponent::gatherSession() const
{
	OneDeckSession session;
	session.deck = restored.deck;
	const auto& host = deckAudio.host();
	session.deck.setId = library.storedId (host.loadedSetId());
	host.gather (session.deck);
	library.saveState (session.library);
	creatorLink->gather (session.stemJobs);
	return session;
}

void OneDeckComponent::saveSession()
{
	analysisCache.flush();   // grid corrections, written with the session
	const auto text = gatherSession().toXml()->toString();
	if (text == lastSessionText)
		return;
	const auto file = oneDeckSessionFile (settings().getFile(), instance);
	file.getParentDirectory().createDirectory();
	if (file.replaceWithText (text))
		lastSessionText = text;
}

//==============================================================================
void OneDeckComponent::updateDeviceStatus()
{
	// Without JACK only the phones play on a device short of all 22 outputs.
	const auto status = audio->status (outputs::oneDeck::channelCount());
	deviceStatus.setText (AudioOutput::withSpatialError (status.text, spatialError), juce::dontSendNotification);
	deviceStatus.setColour (juce::Label::textColourId, AudioOutput::colourOf (status.health));
}

void OneDeckComponent::showSettings()
{
	SettingsPanel::Values values;
	values.libraryFolder = library.getFolder();
	values.audioDeviceChoosable = ! audio->usingJack();
	values.audioDeviceNote = audio->usingJack() ? "Under JACK: route with qjackctl or a patchbay" : juce::String();
	values.tipsShown = tips::shown (settings().getValue (tips::settingKey).toStdString());
	values.phonesMode = phonesMode;

	SettingsPanel::Actions actions;
	actions.onLibraryFolder = [this] (const juce::File& folder) { library.setFolder (folder); };
	actions.onAudioDevice = [this] { audio->showDeviceSettings(); };
	actions.onTips = [this] (bool shown) { settings().setValue (tips::settingKey, juce::String (tips::stored (shown))); };
	actions.onPhonesMode = [this] (spatial::PhonesMode mode)
	{
		phonesMode = mode;
		settings().setValue (phonesModeKey, juce::String (spatial::phonesModeName (mode)));
		updatePhonesDecoder();
	};

	juce::DialogWindow::LaunchOptions dialog;
	dialog.content.setOwned (new SettingsPanel (values, std::move (actions)));
	dialog.dialogTitle = "Settings";
	dialog.dialogBackgroundColour = Theme::panel;
	dialog.useNativeTitleBar = true;
	dialog.resizable = false;
	dialog.launchAsync();
}

void OneDeckComponent::timerCallback()
{
	const auto& player = deckAudio.host().player();
	playButton.setToggleState (player.isPlaying(), juce::dontSendNotification);
	playButton.setEnabled (player.isLoaded());

	updatePhonesDecoder();
	creatorLink->showStatus();
	rec.update (audio->usingJack() ? audio->jackClient().getClientName() : juce::String());

	if (--sessionCountdown <= 0)
	{
		sessionCountdown = 60;   // two seconds at 30 Hz
		saveSession();
	}
	if (--statusCountdown <= 0)
	{
		statusCountdown = 30;
		updateDeviceStatus();
	}
}

//==============================================================================
void OneDeckComponent::paint (juce::Graphics& g)
{
	g.fillAll (Theme::background);
}

void OneDeckComponent::resized()
{
	auto area = getLocalBounds().reduced (8);
	const auto rowHeight = juce::jmax (32, getHeight() / 24);
	const auto gap = 8;

	auto top = area.removeFromTop (rowHeight);
	settingsButton.setBounds (top.removeFromRight (top.getWidth() / 6));
	top.removeFromRight (gap);
	rec.button.setBounds (top.removeFromRight (top.getWidth() / 5));
	top.removeFromRight (gap / 2);
	rec.right.setBounds (top.removeFromRight (8).reduced (0, rowHeight / 10));
	top.removeFromRight (gap / 2);
	rec.left.setBounds (top.removeFromRight (8).reduced (0, rowHeight / 10));
	top.removeFromRight (gap);
	deckLabel.setBounds (top.removeFromLeft (top.getWidth() / 3));
	deviceStatus.setBounds (top);
	area.removeFromTop (gap);

	auto deckRow = area.removeFromTop (rowHeight * 2);
	playButton.setBounds (deckRow.removeFromRight (deckRow.getWidth() / 4));
	deckRow.removeFromRight (gap);
	loadButton.setBounds (deckRow.removeFromRight (deckRow.getWidth() / 4));
	deckRow.removeFromRight (gap);
	setLabel.setBounds (deckRow);
	area.removeFromTop (gap);

	library.setBounds (area);
}
