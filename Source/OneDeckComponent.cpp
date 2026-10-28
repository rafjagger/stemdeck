// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneDeckComponent.h"

#include "AppSettings.h"
#include "KnobSetup.h"
#include "OscTruthFile.h"
#include "SettingsPanel.h"
#include "SurfaceLayout.h"
#include "SyncLabels.h"
#include "Tips.h"

#include <iostream>

namespace
{
double nowSeconds() { return juce::Time::getMillisecondCounterHiRes() / 1000.0; }
constexpr const char* phonesModeKey = "phonesMode";
// Per deck, in the deck's own settings file. "1" take the panel at start if free, "0" never, unset: deck A by default.
constexpr const char* panelHereKey = "panelHere";
constexpr const char* phonesLevelKey = "phonesLevel";
constexpr const char* phonesMixKey = "phonesMix";
constexpr int refreshHz = 60;   // the rolling waveform's
// Shares of the window's height: the top bar and the library (the two-deck screen's 3 % and about 22 %).
constexpr float topBarShare = 0.035f, libraryShare = 0.24f;
}

OneDeckComponent::OneDeckComponent (DeckInstance deck)
	: instance (deck), truth (coretruth::choose()),
	  panelOwnership (appsettings::panelLockFile().getFullPathName().toStdString(), deck.letter)
{
	setLookAndFeel (&lookAndFeel);
	appProperties.setStorageParameters (appsettings::options());
	instanceProperties.setStorageParameters (appsettings::instanceOptions (deck.letter));
	formatManager.registerBasicFormats();
	phonesMode = spatial::phonesModeFrom (instanceSettings().getValue (phonesModeKey).toStdString());

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
	// decks (B15); SYNC's source is always the link here (S4.7), and the
	// button shows what the link is doing.
	autoDjButton.setEnabled (false);
	autoDjButton.setTooltip ("Auto DJ needs two decks: the two-deck StemDeck");
	syncSourceButton.setEnabled (false);
	settingsButton.onClick = [this] { showSettings(); };
	for (auto* button : { &autoDjButton, &syncSourceButton, &settingsButton })
	{
		button->setMouseClickGrabsKeyboardFocus (false);
		addAndMakeVisible (button);
	}

	addAndMakeVisible (view);
	workspaceSwitch.attachTo (*this);
	view.waveform().onSetDropped = [this] (const juce::String& id) { loadById (id); };
	view.deckPanel().onSetDropped = [this] (const juce::String& id) { loadById (id); };
	view.deckPanel().onStep = [this] (int direction) { step (direction); };
	view.outputs().setPhones ((float) instanceSettings().getDoubleValue (phonesLevelKey, 1.0), (float) instanceSettings().getDoubleValue (phonesMixKey, 0.0));
	view.outputs().onPhonesChanged = [this] (float level, float mix)
	{
		instanceSettings().setValue (phonesLevelKey, level);
		instanceSettings().setValue (phonesMixKey, mix);
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
	deckLink = std::make_unique<DeckLink> (instance.oscDeck(), host.spatialPorts().settings().deckThreeD);
	deckLink->start (truth.path, truth.hash);

	startSync();
	setSize (768, 1024);
	restoreSession();
	startPanel();
	updatePhonesDecoder();
	updateDeviceStatus();
	startTimerHz (refreshHz);
}

OneDeckComponent::~OneDeckComponent()
{
	stopTimer();
	linkSender.stop();   // before the player it reads
	link.stop();
	deckLink.reset();   // before the deck whose 3D it writes
	panelDevice.reset();   // dark, and no more presses
	panelOwnership.release();
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
	// Tempo, range, key lock, SYNC (lit only with the link) and each stem's GAIN, MUTE and CUE.
	view.restoreState (deck);
	sync.setSynced (view.deckPanel().isSyncEnabled());
	updateLink();
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
void OneDeckComponent::startSync()
{
	std::string truthError;
	linkPorts = osctruth::proLinkPortsFrom (osctruth::readListeners (truth.path, truthError));

	auto& panel = view.deckPanel();
	panel.setSyncTooltip (syncLabels::linkSyncTooltip());
	panel.setSyncAvailable (linkPorts.complete());
	panel.onSyncToggled = [this] (bool on)
	{
		sync.setSynced (on);
		updateLink();
	};
	panel.onMasterPressed = [this]
	{
		sync.pressMaster (nowSeconds());
		masterChanged();
	};
	showSyncStatus();
}

void OneDeckComponent::masterChanged()
{
	linkSender.setMaster (sync.isMaster() ? &deckAudio.host().player() : nullptr);
	updateLink();
}

// On the link while SYNC or MASTER is on, as the two-deck app; off it otherwise.
void OneDeckComponent::updateLink()
{
	if (! sync.wantsLink())
	{
		linkSender.stop();
		link.stop();
		return;
	}

	const auto device = instance.proLinkDevice();
	if (! link.isRunning() && link.start (device, linkPorts, instance.title()))
		sync.joined (nowSeconds());

	if (sync.isMaster() && link.isRunning() && ! linkSender.isRunning())
		linkSender.start (device, link.broadcastAddress(), linkPorts, instance.title());
	else if (! sync.isMaster())
		linkSender.stop();
}

void OneDeckComponent::showSyncStatus()
{
	// 60 times a second: only a change is written.
	const auto show = [this] (const std::string& text, const std::string& tooltip)
	{
		const auto newText = juce::String::fromUTF8 (text.c_str());
		const auto newTooltip = juce::String::fromUTF8 (tooltip.c_str());
		if (syncSourceButton.getButtonText() != newText)
			syncSourceButton.setButtonText (newText);
		if (syncSourceButton.getTooltip() != newTooltip)
			syncSourceButton.setTooltip (newTooltip);
	};

	if (! linkPorts.complete())
	{
		show (syncLabels::sourceButton (true), syncLabels::linkUnavailable());
		return;
	}

	const auto now = nowSeconds();
	const auto& clock = sync.clock();
	const auto text = ! sync.wantsLink()   ? syncLabels::sourceButton (true)
					: ! link.isRunning() ? syncLabels::error (link.error())
					: sync.isMaster()    ? syncLabels::sendingMaster (instance.letter - 'A')
					: clock.bpm() <= 0.0 ? syncLabels::nothingHeard()
										 : syncLabels::following (clock.bpm(), clock.isLive (now) ? clock.leader (now) : 0);
	show (text, syncLabels::linkSyncTooltip());
}

//==============================================================================
void OneDeckComponent::updateDeviceStatus()
{
	// Without JACK only the phones play on a device short of all 43 outputs.
	const auto status = audio->status (outputs::oneDeck::channelCount());
	deviceStatus.setText (AudioOutput::withSpatialError (status.text, spatialError), juce::dontSendNotification);
	deviceStatus.setColour (juce::Label::textColourId, AudioOutput::colourOf (status.health));
}

//==============================================================================
juce::String OneDeckComponent::panelPreferenceKey() const
{
	return panelHereKey;
}

void OneDeckComponent::startPanel()
{
	const auto key = panelPreferenceKey();
	std::optional<bool> stored;
	if (instanceSettings().containsKey (key))
		stored = instanceSettings().getBoolValue (key);
	panel::claimAtStart (panelOwnership, stored);
	updatePanelOwner();
}

// The lock names the instance with the panel; another instance taking it
// (its toggle) takes it from here: this one lets the port go.
void OneDeckComponent::updatePanelOwner()
{
	const auto holds = panelOwnership.holds();
	if (! holds)
	{
		panelDevice.reset();
		return;
	}
	if (panelDevice != nullptr)
		return;

	auto decoder = std::make_unique<PanelDevice::MappedDecoder<panel1::InputDecoder, panel1::Event>>();
	decoder->onEvent = [this] (const panel1::Event& e) { handlePanel (e); };
	panelDevice = std::make_unique<PanelDevice> (std::move (decoder));
}

// Every press goes through the screen's own control: the panel does what a
// touch on the key does, and nothing here writes the played pose.
void OneDeckComponent::handlePanel (const panel1::Event& e)
{
	using Type = panel1::Event::Type;
	auto& deck = view.deckPanel();

	switch (e.type)
	{
		case Type::cueDown:   deck.cuePressed(); return;
		case Type::cueUp:     deck.cueReleased(); return;
		case Type::play:      deck.togglePlay(); return;
		case Type::shiftDown: view.shift().setToggleState (true, juce::dontSendNotification); return;
		case Type::shiftUp:   view.shift().setToggleState (false, juce::dontSendNotification); return;
		default: break;
	}

	if (e.stem < 0 || e.stem >= StemDeckPlayer::numStems)
		return;
	auto& column = view.column (e.stem);
	switch (e.type)
	{
		case Type::shot:       column.oneShots().press (e.shape); break;
		case Type::stemCue:    column.cue().triggerClick(); break;
		case Type::stemMute:   column.mute().triggerClick(); break;
		case Type::lengthDown: column.oneShots().lengthDown().triggerClick(); break;
		case Type::lengthUp:   column.oneShots().lengthUp().triggerClick(); break;
		case Type::gain:       knobsetup::nudge (column.gain(), e.value); break;
		case Type::threeD:     knobsetup::nudge (column.threeD(), e.value); break;
		default: break;
	}
}

void OneDeckComponent::showPanel()
{
	if (panelDevice == nullptr)
		return;

	const auto& host = deckAudio.host();
	const auto& player = host.player();
	const auto& motion = host.spatialPorts().motion();

	panel1::LedState state;
	for (int s = 0; s < StemDeckPlayer::numStems; ++s)
	{
		auto& stem = state.stems[(size_t) s];
		for (int shape = 0; shape < (int) stem.pads.size(); ++shape)
			stem.pads[(size_t) shape] = motion.padState (s, (motionlite::Shape) shape);
		stem.cued = player.isStemCued (s);
		stem.muted = player.isStemMuted (s);
		const auto bars = motion.lengthBars (s);
		stem.shortest = motionlite::shorterLength (bars) == bars;
		stem.longest = motionlite::longerLength (bars) == bars;
		state.stemColours[(size_t) s] = panel::rgbOf (Theme::stem (s));
	}
	state.deckColour = panel::rgbOf (Theme::deck (instance.letter - 'A'));
	state.cueColour = panel::rgbOf (Theme::cue);
	state.playColour = panel::rgbOf (Theme::play);
	state.muteColour = panel::rgbOf (Theme::mute);
	state.playing = player.isPlaying();
	state.atCue = player.isLoaded() && ! player.isPlaying() && std::abs (player.getPosition() - player.getCuePoint()) < 0.01;
	state.shift = view.shift().getToggleState();
	panelDevice->show (panel1::leds (state, nowSeconds()));
}

void OneDeckComponent::showSettings()
{
	SettingsPanel::Values values;
	values.libraryFolder = library.getFolder();
	values.audioDeviceChoosable = ! audio->usingJack();
	values.audioDeviceNote = audio->usingJack() ? "Under JACK: route with qjackctl or a patchbay" : juce::String();
	values.tipsShown = tips::shown (settings().getValue (tips::settingKey).toStdString());
	values.phonesMode = phonesMode;
	values.panelHere = panelOwnership.holds();

	SettingsPanel::Actions actions;
	actions.onLibraryFolder = [this] (const juce::File& folder) { library.setFolder (folder); };
	actions.onAudioDevice = [this] { audio->showDeviceSettings(); };
	actions.onTips = [this] (bool shown) { settings().setValue (tips::settingKey, juce::String (tips::stored (shown))); };
	actions.onPhonesMode = [this] (spatial::PhonesMode mode)
	{
		phonesMode = mode;
		instanceSettings().setValue (phonesModeKey, juce::String (spatial::phonesModeName (mode)));
		updatePhonesDecoder();
	};

	actions.onPanelHere = [this] (bool here)
	{
		instanceSettings().setValue (panelPreferenceKey(), here);
		if (here)
			panelOwnership.take();
		else
			panelOwnership.release();
		updatePanelOwner();
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
	sync.hear (link.drain());
	if (sync.update (nowSeconds()))
		masterChanged();   // another deck took MASTER
	// SYNC or MASTER on but not on the link (no network yet, ports busy): again every two seconds.
	if (sync.wantsLink() && ! link.isRunning() && --linkRetryCountdown <= 0)
	{
		linkRetryCountdown = 2 * refreshHz;
		updateLink();
	}
	showSyncStatus();
	updatePhonesDecoder();
	if (--panelCountdown <= 0)
	{
		panelCountdown = refreshHz / 2;
		updatePanelOwner();
	}
	showPanel();
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
	top = workspaceSwitch.place (top, gap);   // at the window's right edge, as in the two-deck screen
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
