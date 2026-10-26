// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneDeckComponent.h"

#include "AppSettings.h"
#include "OscTruthFile.h"
#include "SettingsPanel.h"
#include "SurfaceLayout.h"
#include "Tips.h"

#include <iostream>

namespace
{
double nowSeconds() { return juce::Time::getMillisecondCounterHiRes() / 1000.0; }
constexpr const char* phonesModeKey = "phonesMode";
constexpr const char* phonesLevelKey = "phonesLevel";
constexpr const char* phonesMixKey = "phonesMix";
constexpr int refreshHz = 60;   // the rolling waveform's
// Shares of the window's height: the top bar and the library (the two-deck screen's 3 % and about 22 %).
constexpr float topBarShare = 0.035f, libraryShare = 0.24f;
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
		const auto meterParameters = osctruth::readMeterParameters (truth.path, truthError);
		rec.setMeterParameters (meterParameters);
		view.setMeterParameters (meterParameters);
		if (! truthError.empty())
			std::cerr << "StemDeck: " << truthError << std::endl;
	}

	auto& host = deckAudio.host();
	host.onAnalysisStored = [this] { library.analysisChanged(); };
	if (const auto error = deckAudio.createSpatial(); error.isNotEmpty())
		spatialError = error;

	const auto deckColour = Theme::deck (instance.letter - 'A');
	deckLabel.setText ("DECK " + juce::String::charToString ((juce::juce_wchar) instance.letter), juce::dontSendNotification);
	deckLabel.setJustificationType (juce::Justification::centred);
	deckLabel.setColour (juce::Label::backgroundColourId, deckColour);
	deckLabel.setColour (juce::Label::textColourId, juce::Colours::black);
	deviceStatus.setColour (juce::Label::textColourId, Theme::textDim);
	deviceStatus.setMinimumHorizontalScale (0.6f);
	for (auto* label : { &deckLabel, &deviceStatus })
		addAndMakeVisible (label);

	// Shown as the two-deck bar has them, not pressable: Auto DJ needs two
	// decks (B15); SYNC's source and the master are across instances (S4.7).
	autoDjButton.setEnabled (false);
	autoDjButton.setTooltip ("Auto DJ needs two decks: the two-deck StemDeck");
	syncSourceButton.setEnabled (false);
	syncSourceButton.setTooltip ("SYNC across StemDeck instances: not yet");
	settingsButton.onClick = [this] { showSettings(); };
	for (auto* button : { &autoDjButton, &syncSourceButton, &settingsButton })
	{
		button->setMouseClickGrabsKeyboardFocus (false);
		addAndMakeVisible (button);
	}

	addAndMakeVisible (view);
	view.waveform().onSetDropped = [this] (const juce::String& id) { loadById (id); };
	view.deckPanel().onSetDropped = [this] (const juce::String& id) { loadById (id); };
	view.deckPanel().onStep = [this] (int direction) { step (direction); };
	view.outputs().setPhones ((float) settings().getDoubleValue (phonesLevelKey, 1.0), (float) settings().getDoubleValue (phonesMixKey, 0.0));
	view.outputs().onPhonesChanged = [this] (float level, float mix)
	{
		settings().setValue (phonesLevelKey, level);
		settings().setValue (phonesMixKey, mix);
	};

	rec.button.onClick = [this] { rec.toggle (audio->jackClient().getSampleRate()); };
	rec.addTo (*this);

	addAndMakeVisible (library);
	library.setOneDeck (deckColour);
	library.onLoadSet = [this] (const StemSet& set, int) { loadSet (set); };
	library.onOrderChanged = [this] { findSteps(); };
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
	startTimerHz (refreshHz);
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
	view.setSet (set);
	findSteps();
}

void OneDeckComponent::loadById (const juce::String& setId)
{
	if (const auto* set = library.findSet (setId))
		loadSet (*set);
}

// |< >|: the set beside the loaded one in its folder, in the library's order.
void OneDeckComponent::step (int direction)
{
	if (const auto* set = library.neighbourInFolder (deckAudio.host().loadedSetId(), direction))
		loadSet (*set);
}

void OneDeckComponent::findSteps()
{
	const auto& id = deckAudio.host().loadedSetId();
	const auto* previous = library.neighbourInFolder (id, -1);
	const auto* next = library.neighbourInFolder (id, 1);
	view.deckPanel().setStepsAvailable (previous != nullptr, next != nullptr, previous != nullptr ? previous->name : juce::String(),
										next != nullptr ? next->name : juce::String());
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

	if (deck.setId.isNotEmpty())
		if (const auto* set = library.findSet (library.idFromStored (deck.setId)))
			loadSet (*set);
	// Tempo, range, key lock and each stem's GAIN, MUTE and CUE.
	view.restoreState (deck);
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
	view.saveState (session.deck);
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
	view.refresh();
	updatePhonesDecoder();
	creatorLink->showStatus();
	rec.update (audio->usingJack() ? audio->jackClient().getClientName() : juce::String());

	if (--sessionCountdown <= 0)
	{
		sessionCountdown = 2 * refreshHz;   // two seconds
		saveSession();
	}
	if (--statusCountdown <= 0)
	{
		statusCountdown = refreshHz;
		updateDeviceStatus();
	}
}

//==============================================================================
void OneDeckComponent::paint (juce::Graphics& g)
{
	g.fillAll (Theme::background);
}

// The top bar, the deck (OneDeckView), the library: shares of the window,
// as the two-deck screen's (SurfaceLayout).
void OneDeckComponent::resized()
{
	const auto gap = surface::metrics (getWidth()).gap;
	auto area = getLocalBounds().reduced (gap);
	const auto barHeight = juce::roundToInt ((float) getHeight() * topBarShare);
	const auto key = [&area] (float share) { return juce::roundToInt ((float) area.getWidth() * share); };

	auto top = area.removeFromTop (barHeight);
	deckLabel.setFont (juce::FontOptions ((float) barHeight * 0.6f, juce::Font::bold));
	deckLabel.setBounds (top.removeFromLeft (key (0.13f)));
	top.removeFromLeft (gap);
	rec.button.setBounds (top.removeFromLeft (key (0.09f)));
	top.removeFromLeft (gap / 2);
	const auto meterWidth = juce::jmax (2, gap);
	rec.left.setBounds (top.removeFromLeft (meterWidth).reduced (0, barHeight / 10));
	top.removeFromLeft (gap / 2);
	rec.right.setBounds (top.removeFromLeft (meterWidth).reduced (0, barHeight / 10));
	top.removeFromLeft (gap);
	autoDjButton.setBounds (top.removeFromLeft (key (0.12f)));
	top.removeFromLeft (gap);
	syncSourceButton.setBounds (top.removeFromLeft (key (0.15f)));
	settingsButton.setBounds (top.removeFromRight (key (0.13f)));
	top.removeFromRight (gap);
	top.removeFromLeft (gap);
	deviceStatus.setBounds (top);
	area.removeFromTop (gap);

	library.setBounds (area.removeFromBottom (juce::roundToInt ((float) getHeight() * libraryShare)));
	area.removeFromBottom (gap);
	view.setBounds (area);
}
