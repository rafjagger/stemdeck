#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
	setLookAndFeel (&lookAndFeel);

	juce::PropertiesFile::Options options;
	options.applicationName = "StemDeck";
   #if JUCE_LINUX
	options.folderName = "~/.config/StemDeck";
   #else
	options.folderName = "StemDeck";
   #endif
	options.filenameSuffix = ".settings";
	options.osxLibrarySubFolder = "Application Support";
	appProperties.setStorageParameters (options);
	analysisCache = std::make_unique<AnalysisCache> (appProperties.getUserSettings()->getFile().getSiblingFile ("analysis.xml"));

	formatManager.registerBasicFormats();

	for (int d = 0; d < numDecks; ++d)
	{
		addAndMakeVisible (waves[(size_t) d]);
		addAndMakeVisible (decks[(size_t) d]);
		waves[(size_t) d]->onSetDropped = [this, d] (const juce::String& id) { loadDroppedSet (id, d); };
		decks[(size_t) d]->onSetDropped = [this, d] (const juce::String& id) { loadDroppedSet (id, d); };
		decks[(size_t) d]->onSyncToggled = [this, d] (bool enabled) { setSync (d, enabled); };
	}

	addAndMakeVisible (mixer);
	addAndMakeVisible (library);

	library.onLoadSet = [this] (const StemSet& set, int deckIndex) { loadSet (set, deckIndex); };
	library.lookUpBeatGrid = [this] (const StemSet& set) { return analysisCache->find (set); };
	library.onFolderChanged = [this] (const juce::File& folder)
	{
		appProperties.getUserSettings()->setValue ("stemFolder", folder.getFullPathName());
	};

	// Last used folder, otherwise ./stems next to where the app was started.
	const auto savedFolder = appProperties.getUserSettings()->getValue ("stemFolder");
	library.setFolder (savedFolder.isNotEmpty() ? juce::File (savedFolder)
												: juce::File::getCurrentWorkingDirectory().getChildFile ("stems"));

	audioSettingsButton.onClick = [this] { showAudioSettings(); };
	audioSettingsButton.setMouseClickGrabsKeyboardFocus (false);
	addAndMakeVisible (audioSettingsButton);
	deviceStatus.setColour (juce::Label::textColourId, Theme::textDim);
	addAndMakeVisible (deviceStatus);

	// Mixxx-like keys: D/L play, S/K cue (hold to preview), 1-4 / 7-0 stem mutes.
	const auto bind = [this] (int key, std::function<void (bool)> action) { keyBindings.push_back ({ key, std::move (action) }); };
	bind ('d', [this] (bool down) { if (down) deckA.togglePlay(); });
	bind ('l', [this] (bool down) { if (down) deckB.togglePlay(); });
	bind ('s', [this] (bool down) { down ? deckA.cuePressed() : deckA.cueReleased(); });
	bind ('k', [this] (bool down) { down ? deckB.cuePressed() : deckB.cueReleased(); });

	for (int s = 0; s < StemSet::numStems; ++s)
	{
		bind ('1' + s, [this, s] (bool down) { if (down) mixer.strip (0).toggleMute (s); });
		bind ("7890"[s], [this, s] (bool down) { if (down) mixer.strip (1).toggleMute (s); });
	}

	initialiseAudio();

	setWantsKeyboardFocus (true);
	setSize (1500, 960);
	startTimerHz (60);
}

MainComponent::~MainComponent()
{
	stopTimer();
	analysisPool.removeAllJobs (true, 5000);
	jack.close();
	deviceManager.removeChangeListener (this);
	deviceManager.removeAudioCallback (&audioSourcePlayer);
	audioSourcePlayer.setSource (nullptr);
	deviceManager.closeAudioDevice();
	setLookAndFeel (nullptr);
}

//==============================================================================
void MainComponent::loadSet (const StemSet& set, int deckIndex)
{
	// No deck given (double-click, Enter): first deck that isn't playing.
	if (deckIndex < 0)
	{
		for (int d = 0; d < numDecks && deckIndex < 0; ++d)
			if (! players[(size_t) d]->isPlaying())
				deckIndex = d;

		if (deckIndex < 0)
			return;
	}

	const auto d = (size_t) deckIndex;
	const auto error = players[d]->load (set);

	if (error.isNotEmpty())
	{
		juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Set konnte nicht geladen werden", error);
		return;
	}

	thumbs[d]->setSet (set);
	decks[d]->setSet (set);
	waves[d]->setTitle (set.name);
	mixer.strip (deckIndex).setStemNames (set.stemNames);

	++loadGeneration[d];

	if (const auto cached = analysisCache->find (set))
	{
		players[d]->setBeatGrid (*cached);
		decks[d]->setAnalysing (false);
	}
	else
	{
		startAnalysis (set, deckIndex);
	}
}

void MainComponent::startAnalysis (const StemSet& set, int deckIndex)
{
	decks[(size_t) deckIndex]->setAnalysing (true);
	const auto generation = loadGeneration[(size_t) deckIndex];
	juce::Component::SafePointer<MainComponent> safeThis (this);

	analysisPool.addJob ([this, safeThis, set, deckIndex, generation]
	{
		auto* job = juce::ThreadPoolJob::getCurrentThreadPoolJob();
		const auto grid = TempoAnalysis::analyse (set, formatManager, [job] { return job != nullptr && job->shouldExit(); });

		juce::MessageManager::callAsync ([safeThis, set, deckIndex, generation, grid]
		{
			if (safeThis == nullptr)
				return;

			if (grid.isValid())
			{
				safeThis->analysisCache->store (set, grid);
				safeThis->library.analysisChanged();
			}

			// Only apply it if the deck still has that set.
			if (safeThis->loadGeneration[(size_t) deckIndex] == generation)
			{
				safeThis->players[(size_t) deckIndex]->setBeatGrid (grid);
				safeThis->decks[(size_t) deckIndex]->setAnalysing (false);
			}
		});
	});
}

//==============================================================================
void MainComponent::setSync (int deckIndex, bool enabled)
{
	if (enabled)
	{
		// Only one follower: the other deck becomes the leader.
		const auto other = 1 - deckIndex;
		decks[(size_t) other]->setSyncEnabled (false);
		players[(size_t) other]->setSyncNudge (1.0);
		syncFollower = deckIndex;
		syncMultiple = 0.0; // chosen on the first update with both grids known
		updateSync();
	}
	else if (syncFollower == deckIndex)
	{
		players[(size_t) deckIndex]->setSyncNudge (1.0);
		syncFollower = -1;
	}
}

// Keeps the follower at the leader's tempo and nudges its beats into phase.
void MainComponent::updateSync()
{
	if (syncFollower < 0)
		return;

	auto& follower = *players[(size_t) syncFollower];
	auto& leader = *players[(size_t) (1 - syncFollower)];
	const auto followerGrid = follower.getBeatGrid();
	const auto leaderGrid = leader.getBeatGrid();

	if (! followerGrid.isValid() || ! leaderGrid.isValid())
		return;

	FollowInput in;
	in.leaderBpm = leaderGrid.bpm * leader.getSpeed();
	in.leaderBeatPhase = leaderGrid.beatsAt (leader.getPosition());
	in.leaderPlaying = leader.isPlaying();
	in.anyScratching = follower.isScratching() || leader.isScratching();
	in.multiple = syncMultiple;

	syncMultiple = applyFollow (syncFollower, in);
}

// One synced deck against its leader: the rules in followLeader(), the
// result put on the deck.
double MainComponent::applyFollow (int deckIndex, FollowInput in)
{
	auto& follower = *players[(size_t) deckIndex];
	const auto grid = follower.getBeatGrid();

	in.followerGridBpm = grid.bpm;
	in.followerBeatPhase = grid.beatsAt (follower.getPosition());
	in.followerSpeed = follower.getSpeed();
	in.followerEffectiveRate = follower.getEffectiveRate();
	in.followerPlaying = follower.isPlaying();

	const auto out = followLeader (in);

	if (out.tempo)
		decks[(size_t) deckIndex]->setTempoFromSync (*out.tempo);

	if (out.jumpBeats)
		follower.setPosition (follower.getPosition() + *out.jumpBeats * grid.beatLength());

	follower.setSyncNudge (out.nudge);
	return out.multiple;
}

void MainComponent::loadDroppedSet (const juce::String& setId, int deckIndex)
{
	if (const auto* set = library.findSet (setId))
		loadSet (*set, deckIndex);
}

//==============================================================================
void MainComponent::initialiseAudio()
{
	juce::StringArray portNames;

	for (int bus = 0; bus < numBuses; ++bus)
	{
		const auto name = bus == auxBus ? juce::String ("aux") : "deck" + juce::String (bus + 1);
		portNames.addArray ({ name + "_L", name + "_R" });
	}

	usingJack = jack.open ("StemDeck", portNames, *this).isEmpty();

	if (! usingJack)
		initialiseDeviceManager();

	audioSettingsButton.setEnabled (! usingJack);
	audioSettingsButton.setTooltip (usingJack ? "Unter JACK: Routing per qjackctl oder Patchbay" : juce::String());
	updateDeviceStatus();
}

void MainComponent::initialiseDeviceManager()
{
	const auto numOutputs = numOutputChannels;
	const auto savedState = appProperties.getUserSettings()->getXmlValue ("audioDeviceState");

	deviceManager.initialise (0, numOutputs, savedState.get(), true);

	// First start: enable as many of the 10 outputs as the device has.
	if (savedState == nullptr)
	{
		auto setup = deviceManager.getAudioDeviceSetup();
		setup.useDefaultOutputChannels = false;
		setup.outputChannels.clear();
		setup.outputChannels.setRange (0, numOutputs, true);
		deviceManager.setAudioDeviceSetup (setup, true);
	}

	deviceManager.addChangeListener (this);
	audioSourcePlayer.setSource (this);
	deviceManager.addAudioCallback (&audioSourcePlayer);
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
	if (auto state = deviceManager.createStateXml())
		appProperties.getUserSettings()->setValue ("audioDeviceState", state.get());

	updateDeviceStatus();
}

void MainComponent::updateDeviceStatus()
{
	if (usingJack)
	{
		const bool running = jack.isRunning();
		juce::String text;

		if (running)
			text << "JACK: " << jack.getClientName() << "  |  " << juce::String (jack.getSampleRate(), 0) << " Hz, "
				 << jack.getBufferSize() << " Samples  |  " << jack.getNumPorts() << " Ports, "
				 << jack.getNumConnectedPorts() << " verbunden  |  Xruns: " << jack.getXrunCount();
		else
			text = "JACK-Server wurde beendet - bitte StemDeck neu starten";

		deviceStatus.setText (text, juce::dontSendNotification);
		deviceStatus.setColour (juce::Label::textColourId, running ? Theme::textDim : Theme::mute);
		return;
	}

	auto* device = deviceManager.getCurrentAudioDevice();

	if (device == nullptr)
	{
		deviceStatus.setText (juce::String::fromUTF8 ("Kein Audioger\xc3\xa4t offen"), juce::dontSendNotification);
		deviceStatus.setColour (juce::Label::textColourId, Theme::mute);
		return;
	}

	const auto outputs = device->getActiveOutputChannels().countNumberOfSetBits();
	auto text = deviceManager.getCurrentAudioDeviceType() + ": " + device->getName()
			  + "  |  " + juce::String (device->getCurrentSampleRate(), 0) + " Hz, "
			  + juce::String (device->getCurrentBufferSizeSamples()) + " Samples  |  "
			  + juce::String (outputs) + juce::String::fromUTF8 (" Ausg\xc3\xa4nge");

	if (outputs < numOutputChannels)
		text << "  (Busse werden zusammengemischt)";

	deviceStatus.setText (text, juce::dontSendNotification);
	deviceStatus.setColour (juce::Label::textColourId, outputs < numOutputChannels ? Theme::cue : Theme::textDim);
}

void MainComponent::showAudioSettings()
{
	auto selector = std::make_unique<juce::AudioDeviceSelectorComponent> (
		deviceManager, 0, 0, 2, numOutputChannels, false, false, true, false);
	selector->setSize (520, 460);

	juce::DialogWindow::LaunchOptions dialog;
	dialog.content.setOwned (selector.release());
	dialog.dialogTitle = "Audio-Einstellungen";
	dialog.dialogBackgroundColour = Theme::panel;
	dialog.useNativeTitleBar = true;
	dialog.resizable = true;
	dialog.launchAsync();
}

void MainComponent::timerCallback()
{
	for (int d = 0; d < numDecks; ++d)
	{
		waves[(size_t) d]->repaint();
		decks[(size_t) d]->refresh();
	}

	mixer.refresh();
	updateSync();

	for (int ch = 0; ch < numOutputChannels; ++ch)
		mixer.setOutputLevel (ch, outputPeaks[(size_t) ch].exchange (0.0f));

	if (--statusCountdown <= 0)
	{
		statusCountdown = 60;
		updateDeviceStatus();
	}
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
	const auto maxBlock = juce::jmax (samplesPerBlockExpected, 4096);
	busBuffer.setSize (numOutputChannels, maxBlock);

	for (int d = 0; d < numDecks; ++d)
	{
		deckBuffers[(size_t) d].setSize (StemDeckPlayer::numOutputChannels, maxBlock);
		players[(size_t) d]->prepareToPlay (samplesPerBlockExpected, sampleRate);

		for (int s = 0; s < StemSet::numStems; ++s)
		{
			auto& amount = auxAmounts[(size_t) d][(size_t) s];
			amount.reset (sampleRate, 0.01);
			amount.setCurrentAndTargetValue (players[(size_t) d]->isStemToAux (s) ? 1.0f : 0.0f);
		}
	}
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
	auto& out = *bufferToFill.buffer;
	const auto numSamples = bufferToFill.numSamples;
	const auto numOut = out.getNumChannels();

	if (numSamples > busBuffer.getNumSamples())
		busBuffer.setSize (numOutputChannels, numSamples, false, false, true);

	busBuffer.clear (0, numSamples);

	for (int d = 0; d < numDecks; ++d)
	{
		auto& deckBuffer = deckBuffers[(size_t) d];
		auto& player = *players[(size_t) d];

		if (numSamples > deckBuffer.getNumSamples())
			deckBuffer.setSize (StemDeckPlayer::numOutputChannels, numSamples, false, false, true);

		player.getNextAudioBlock (juce::AudioSourceChannelInfo (&deckBuffer, 0, numSamples));

		// Stem N -> bus N, or -> aux bus (post fader), crossfaded over 10 ms on change.
		for (int s = 0; s < StemSet::numStems; ++s)
		{
			auto& amount = auxAmounts[(size_t) d][(size_t) s];
			amount.setTargetValue (player.isStemToAux (s) ? 1.0f : 0.0f);
			const auto from = amount.getCurrentValue();
			amount.skip (numSamples);
			const auto to = amount.getCurrentValue();

			for (int c = 0; c < 2; ++c)
			{
				const auto* source = deckBuffer.getReadPointer (s * 2 + c);
				busBuffer.addFromWithRamp (s * 2 + c, 0, source, numSamples, 1.0f - from, 1.0f - to);
				busBuffer.addFromWithRamp (auxBus * 2 + c, 0, source, numSamples, from, to);
			}
		}
	}

	for (int ch = 0; ch < numOutputChannels; ++ch)
	{
		const auto peak = busBuffer.getMagnitude (ch, 0, numSamples);

		if (peak > outputPeaks[(size_t) ch].load())
			outputPeaks[(size_t) ch] = peak;
	}

	// One output per bus channel; with fewer outputs (e.g. plain stereo for
	// monitoring) the buses are summed down.
	bufferToFill.clearActiveBufferRegion();

	if (numOut > 0)
		for (int ch = 0; ch < numOutputChannels; ++ch)
			out.addFrom (ch % numOut, bufferToFill.startSample, busBuffer, ch, 0, numSamples);
}

void MainComponent::releaseResources()
{
	for (auto* player : players)
		player->releaseResources();
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
	g.fillAll (Theme::background);
}

void MainComponent::resized()
{
	auto area = getLocalBounds().reduced (6);

	auto topBar = area.removeFromTop (30);
	audioSettingsButton.setBounds (topBar.removeFromRight (160));
	deviceStatus.setBounds (topBar);
	area.removeFromTop (4);

	const auto waveHeight = juce::jlimit (90, 180, getHeight() / 7);
	waveA.setBounds (area.removeFromTop (waveHeight));
	area.removeFromTop (3);
	waveB.setBounds (area.removeFromTop (waveHeight));
	area.removeFromTop (6);

	auto middle = area.removeFromTop (juce::jmin (380, area.getHeight() - 150));
	const auto mixerWidth = juce::jlimit (460, 560, getWidth() / 3);
	const auto deckWidth = (middle.getWidth() - mixerWidth) / 2;
	deckA.setBounds (middle.removeFromLeft (deckWidth));
	deckB.setBounds (middle.removeFromRight (deckWidth));
	mixer.setBounds (middle);
	area.removeFromTop (6);

	library.setBounds (area);
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
	// Handled edge-triggered in keyStateChanged; swallow the repeats here.
	for (const auto& binding : keyBindings)
		if (key.getTextCharacter() == binding.key)
			return true;

	return false;
}

bool MainComponent::keyStateChanged (bool)
{
	// Typing in the search box must not trigger the decks.
	if (dynamic_cast<juce::TextEditor*> (getCurrentlyFocusedComponent()) != nullptr)
		return false;

	bool handled = false;

	for (auto& binding : keyBindings)
	{
		const bool down = juce::KeyPress::isKeyCurrentlyDown (binding.key)
					   || juce::KeyPress::isKeyCurrentlyDown (juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) binding.key));

		if (down != binding.wasDown)
		{
			binding.wasDown = down;
			binding.action (down);
			handled = true;
		}
	}

	return handled;
}
