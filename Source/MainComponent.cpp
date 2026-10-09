#include "MainComponent.h"
#include "AudioPanel.h"
#include "StemJob.h"
#include "GridEdit.h"
#include "OscTruthFile.h"
#include "SyncLabels.h"
#include "Tips.h"
#include "SurfaceJuce.h"

#include <cstdlib>
#include <iostream>
#include <pthread.h>
#include <sched.h>

//==============================================================================
MainComponent::MainComponent()
{
	setLookAndFeel (&lookAndFeel);

	// The one truth, chosen once and hashed at once (spec truth-from-core):
	// the keeper's own fingerprint must be of this truth, not of a cache
	// Motion may write a moment later.
	truthPath = osctruth::liveTruthPath();
	truthHash = juce::SHA256 (juce::File (truthPath)).toHexString();

	// Where Pro DJ Link listens: the one truth. Without it the PIO clock
	// does not start and its status line says why.
	{
		std::string truthError;
		proLinkPorts = osctruth::proLinkPortsFrom (osctruth::readListeners (truthPath, truthError));
		if (! truthError.empty())
			std::cerr << "StemDeck: " << truthError << std::endl;
	}

	// How every meter on screen moves: Core's numbers, one set for the
	// system (decided 2026-10-07). A new truth restarts StemDeck, so once is enough.
	{
		std::string truthError;
		const auto meterParameters = osctruth::readMeterParameters (truthPath, truthError);
		mixer.setMeterParameters (meterParameters);
		recMeterL.setParameters (meterParameters);
		recMeterR.setParameters (meterParameters);
	}

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
	showTips (tips::shown (settings().getValue (tips::settingKey).toStdString()));

	for (int d = 0; d < numDecks; ++d)
	{
		addAndMakeVisible (waves[(size_t) d]);
		addAndMakeVisible (decks[(size_t) d]);
		decks[(size_t) d]->addBandPartsTo (*this);
		waves[(size_t) d]->onSetDropped = [this, d] (const juce::String& id) { loadDroppedSet (id, d); };
		decks[(size_t) d]->onSetDropped = [this, d] (const juce::String& id) { loadDroppedSet (id, d); };
		decks[(size_t) d]->onSyncToggled = [this, d] (bool enabled) { setSync (d, enabled); };
		decks[(size_t) d]->onGridEdit = [this, d] (DeckPanel::GridAction action, double seconds) { editGrid (d, action, seconds); };
		decks[(size_t) d]->onMasterPressed = [this, d] { pressMaster (d); };
		decks[(size_t) d]->onStep = [this, d] (int direction) { stepDeck (d, direction); };
	}

	addAndMakeVisible (mixer);
	mixer.addBandPartsTo (*this);
	addAndMakeVisible (library);

	library.onLoadSet = [this] (const StemSet& set, int deckIndex) { loadSet (set, deckIndex); };
	library.lookUpBeatGrid = [this] (const StemSet& set) { return analysisCache->find (set); };
	library.onFolderChanged = [this] (const juce::File& folder)
	{
		appProperties.getUserSettings()->setValue ("stemFolder", folder.getFullPathName());
		stemCreator.setLibraryFolder (folder);
	};
	library.onCreateStems = [this] (const juce::Array<juce::File>& files) { createStems (files); };
	library.onCancelCreation = [this] { stemCreator.cancelRunning(); };
	library.onOrderChanged = [this] { findSteps(); };
	stemCreator.onSetCreated = [this] (const juce::File& firstStem)
	{
		library.setFolder (library.getFolder());
		library.selectSetWithFile (firstStem);
	};
	if (const auto venv = appProperties.getUserSettings()->getValue ("separatorVenv"); juce::File::isAbsolutePath (venv))
		stemCreator.setVenv (venv);
	// All cores but the audio CPU, unless the settings say fewer.
	if (const auto cores = appProperties.getUserSettings()->getIntValue ("separatorCores"); cores > 0)
		stemCreator.setMaxCores (cores);

	// Last used folder, otherwise ./stems next to where the app was started.
	// A saved folder that is gone (the checkout moved) falls back too, or the
	// library stays empty with nothing to say why.
	const auto savedFolder = appProperties.getUserSettings()->getValue ("stemFolder");
	const juce::File saved (savedFolder.isNotEmpty() && juce::File::isAbsolutePath (savedFolder) ? savedFolder : juce::String());
	library.setFolder (saved.isDirectory() ? saved
										   : juce::File::getCurrentWorkingDirectory().getChildFile ("stems"));
	stemCreator.setLibraryFolder (library.getFolder());

	settingsButton.onClick = [this] { showSettings(); };
	settingsButton.setMouseClickGrabsKeyboardFocus (false);
	addAndMakeVisible (settingsButton);
	// The on-screen keyboard for a rig without a real one. The button must not
	// take the focus, or pressing it would leave the text field it is for.
	keysButton.setTooltip (keyboard.isAvailable() ? "On-screen keyboard (onboard): show or hide"
												  : "On-screen keyboard: onboard is not installed");
	keysButton.onClick = [this] { keyboard.keysPressed(); };
	keysButton.setMouseClickGrabsKeyboardFocus (false);
	addAndMakeVisible (keysButton);
	keyboard.onHint = [this] (const juce::String& hint)
	{
		keysButton.setTooltip (hint);
		deviceStatus.setText (hint, juce::dontSendNotification);
		deviceStatus.setColour (juce::Label::textColourId, Theme::cue);
		statusCountdown = 240;   // four seconds at 60 Hz, then the device again
	};
	motionButton.setTooltip ("Over to A3 Motion (i3 workspace 1)");
	motionButton.onClick = [this] { goToWorkspace (1); };
	workspacesButton.setTooltip ("Any of the rig's workspaces");
	workspacesButton.onClick = [this] { showWorkspaces(); };
	for (auto* b : { &motionButton, &workspacesButton })
	{
		b->setMouseClickGrabsKeyboardFocus (false);
		addAndMakeVisible (b);
	}
	workspacePanel.onChosen = [this] (int number) { goToWorkspace (number); };
	addChildComponent (workspacePanel);
	deviceStatus.setColour (juce::Label::textColourId, Theme::textDim);
	addAndMakeVisible (deviceStatus);

	recButton.setMouseClickGrabsKeyboardFocus (false);
	recButton.setColour (juce::TextButton::buttonOnColourId, Theme::mute);
	recButton.onClick = [this] { toggleRecording(); };
	addAndMakeVisible (recButton);
	addAndMakeVisible (recMeterL);
	addAndMakeVisible (recMeterR);

	autoDjButton.setClickingTogglesState (true);
	autoDjButton.setMouseClickGrabsKeyboardFocus (false);
	autoDjButton.setColour (juce::TextButton::buttonOnColourId, Theme::play);
	setAutoDjFade ({ settings().getIntValue ("autoDjOverlapBars", AutoDj::defaultMixBars),
					 settings().getDoubleValue ("autoDjMixSeconds", AutoDj::defaultNoGridMixSeconds) });
	autoDjButton.onClick = [this] { setAutoDj (autoDjButton.getToggleState()); };
	addAndMakeVisible (autoDjButton);

	// SYNC source: the other deck, or the Pioneer tempo master (PIO).
	syncSourceButton.setClickingTogglesState (true);
	syncSourceButton.setMouseClickGrabsKeyboardFocus (false);
	syncSourceButton.onClick = [this] { setSyncSource (syncSourceButton.getToggleState()); };
	addAndMakeVisible (syncSourceButton);
	pioStatus.setColour (juce::Label::textColourId, Theme::textDim);
	addAndMakeVisible (pioStatus);
	for (int player = 1; player <= 4; ++player)
		pioPlayer.addItem (syncLabels::player (player), player);
	pioPlayer.setTooltip ("No master info: the player to follow");
	pioPlayer.onChange = [this]
	{
		pioClock.choosePlayer (pioPlayer.getSelectedId());
		appProperties.getUserSettings()->setValue ("pioPlayer", pioPlayer.getSelectedId());
	};
	addChildComponent (pioPlayer);
	if (const auto player = appProperties.getUserSettings()->getIntValue ("pioPlayer", 0); player > 0)
		pioClock.choosePlayer (player);

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

	if (appProperties.getUserSettings()->getValue ("syncSource") == "pio")
	{
		syncSourceButton.setToggleState (true, juce::dontSendNotification);
		setSyncSource (true);
	}
	updatePioneerStatus();

	setWantsKeyboardFocus (true);
	setSize (1500, 960);
	restoreSession();
	startTimerHz (60);

	// After the session is restored: a click reports, a restore before the
	// link runs reports nothing, and Core asks for everything on our hello.
	mixer.onBusesChanged = [this] (int deck, int stem) { remote.report (deck, stem); };
	static_assert (AutoDj::auxBus == buses::aux && AutoDj::numStems == buses::stemsPerDeck);
	mixer.onDjSwitch = [this] (int deck, int stem, int bus, bool on) { autoDj.djSwitched (deck, stem, bus, on); };
	remote.start (truthPath, truthHash);

	motionPanel = std::make_unique<PanelDevice>();
	motionPanel->onEvent = [this] (const panel::Event& e) { handlePanel (e); };

	// Following Core's truth: with $A3_OSC_TRUTH set, that file wins at every
	// start, and following would restart StemDeck into it forever.
	if (truthkeeper::followsCore (std::getenv ("A3_OSC_TRUTH")))
	{
		const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName().toStdString();
		truthKeeper = std::make_unique<TruthKeeperLink> (
			truthHash.toStdString(),
			juce::File (truthkeeper::cachePath (home)),
			[] (const juce::String& body) { return juce::String (osctruth::unusableTruth (body.toStdString())); },
			[] {
				juce::JUCEApplication::getInstance()->setApplicationReturnValue (1);
				juce::JUCEApplication::quit();
			});
		// radla's subnet never hears the broadcast: there a file names Core.
		const auto core = juce::File (truthkeeper::corePath (home));
		const auto url = core.existsAsFile() ? truthkeeper::pollUrl (core.loadFileAsString().toStdString()) : std::string();
		if (url.empty())
		{
			if (core.existsAsFile() && core.loadFileAsString().trim().isNotEmpty())
				std::cerr << "StemDeck: " << core.getFullPathName() << " does not name an http:// address;"
						  << " listening for Core's broadcast instead" << std::endl;
			truthKeeper->start();
		}
		else
		{
			std::cerr << "StemDeck: following Core's truth at " << url << " every "
					  << truthkeeper::pollSeconds << " s" << std::endl;
			truthKeeper->startPolling (juce::String (url));
		}
	}
	else
		std::cerr << "StemDeck: A3_OSC_TRUTH is set: not following Core's truth" << std::endl;
}

MainComponent::~MainComponent()
{
	stopTimer();
	motionPanel.reset();   // dark, and no more presses
	saveSession();
	jack.close();        // no more input blocks ...
	recorder.stop();     // ... then the file is closed complete
	pioSender.setMaster (nullptr);
	pioSender.stop();   // before the players it reads go away
	proLink.stop();
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
		juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Cannot load the set", error);
		return;
	}

	loadedSetIds[d] = set.files[0].getFullPathName();
	loadedSets[d] = set;
	thumbs[d]->setSet (set);
	decks[d]->setSet (set);
	waves[d]->setTitle (set.name);
	findSteps();

	++loadGeneration[d];
	sectionFeatures[d].reset();
	sectionBars[d].clear();

	if (const auto cached = analysisCache->find (set))
	{
		players[d]->setBeatGrid (*cached);
		decks[d]->setAnalysing (false);

		// A set analysed before the sections existed gets them now.
		sectionFeatures[d] = analysisCache->findFeatures (set);
		if (sectionFeatures[d])
			rebuildSections (deckIndex);
		else
			startSectionAnalysis (set, deckIndex, *cached);
		if (analysisCache->needsNewDownbeat (set))
			startDownbeatAnalysis (set, deckIndex);
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

				if (grid.isValid())
					safeThis->startSectionAnalysis (set, deckIndex, grid);
			}
		});
	});
}

void MainComponent::startSectionAnalysis (const StemSet& set, int deckIndex, const BeatGrid& grid)
{
	const auto generation = loadGeneration[(size_t) deckIndex];
	juce::Component::SafePointer<MainComponent> safeThis (this);

	analysisPool.addJob ([this, safeThis, set, deckIndex, generation, grid]
	{
		auto* job = juce::ThreadPoolJob::getCurrentThreadPoolJob();
		const auto features = SectionAnalysis::analyse (set, formatManager, grid,
														[job] { return job != nullptr && job->shouldExit(); });

		juce::MessageManager::callAsync ([safeThis, set, deckIndex, generation, features]
		{
			if (safeThis == nullptr || ! features)
				return;

			safeThis->analysisCache->storeFeatures (set, *features);

			// Only apply them if the deck still has that set.
			if (safeThis->loadGeneration[(size_t) deckIndex] != generation)
				return;

			safeThis->sectionFeatures[(size_t) deckIndex] = features;
			safeThis->rebuildSections (deckIndex);
		});
	});
}

// A grid from before the one was found from the stems is played with at
// once; its first beat, onto the kick and the one, follows a few seconds later.
void MainComponent::startDownbeatAnalysis (const StemSet& set, int deckIndex)
{
	const auto analysed = analysisCache->findAnalysed (set);
	if (! analysed)
		return;

	const auto generation = loadGeneration[(size_t) deckIndex];
	juce::Component::SafePointer<MainComponent> safeThis (this);

	analysisPool.addJob ([this, safeThis, set, deckIndex, generation, grid = *analysed]
	{
		auto* job = juce::ThreadPoolJob::getCurrentThreadPoolJob();
		const auto found = TempoAnalysis::redetectDownbeat (set, formatManager, grid, [job] { return job != nullptr && job->shouldExit(); });

		juce::MessageManager::callAsync ([safeThis, set, deckIndex, generation, found]
		{
			if (safeThis == nullptr || ! found.isValid())
				return;

			safeThis->analysisCache->storeNewDownbeat (set, found.firstBeat);
			safeThis->library.analysisChanged();

			// A correction made meanwhile is what find() returns, and stays.
			if (safeThis->loadGeneration[(size_t) deckIndex] == generation)
				if (const auto current = safeThis->analysisCache->find (set))
				{
					safeThis->players[(size_t) deckIndex]->setBeatGrid (*current);
					// The sections are counted in bars from the one.
					safeThis->rebuildSections (deckIndex);
				}
		});
	});
}

void MainComponent::rebuildSections (int deckIndex)
{
	const auto d = (size_t) deckIndex;
	const auto grid = players[d]->getBeatGrid();

	if (! sectionFeatures[d] || ! loadedSets[d] || ! grid.isValid())
	{
		sectionBars[d].clear();
		return;
	}

	const auto& names = loadedSets[d]->stemNames;
	const auto roles = sections::rolesFor ({ names[0].toStdString(), names[1].toStdString(),
											 names[2].toStdString(), names[3].toStdString() });
	sectionBars[d] = sections::classify (sections::barLevels (*sectionFeatures[d], grid.bpm, grid.firstBeat, roles));
}

void MainComponent::sendPreview()
{
	std::array<preview::DeckState, numDecks> states;

	for (int d = 0; d < numDecks; ++d)
	{
		const auto& player = *players[(size_t) d];
		const auto grid = player.getBeatGrid();
		auto& state = states[(size_t) d];
		state.view = { player.isPlaying(), player.getDeckGain() };
		state.generation = loadGeneration[(size_t) d];
		state.speed = player.getSpeed();
		state.bpm = grid.bpm;
		state.firstBeat = grid.firstBeat;
		state.position = player.getPosition();
		if (player.hasLoop())
			state.loopSeconds = std::make_pair (player.getLoop().getStart(), player.getLoop().getEnd());
		state.bars = sectionBars[(size_t) d];
	}

	const auto moment = preview::momentOf (states, masterDeck);
	if (previewGate.shouldSend (moment, juce::Time::getMillisecondCounterHiRes() / 1000.0))
		remote.sendAhead (moment.ahead);
}

//==============================================================================
void MainComponent::setSyncBent (int deckIndex, bool bent)
{
	syncBent[(size_t) deckIndex] = bent;
	decks[(size_t) deckIndex]->setSyncBpmOnly (bent);
	if (bent)
		players[(size_t) deckIndex]->setSyncNudge (1.0);
}

void MainComponent::setSync (int deckIndex, bool enabled)
{
	setSyncBent (deckIndex, false);

	if (pioSource)
	{
		// Every deck follows the master on its own; both may be on at once.
		pioSynced[(size_t) deckIndex] = enabled;
		pioMultiple[(size_t) deckIndex] = 0.0; // chosen on the first update
		if (! enabled)
			players[(size_t) deckIndex]->setSyncNudge (1.0);
		return;
	}

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
	if (pioSource)
	{
		followPioneer();
		return;
	}

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
	in.alignBars = true;   // deck to deck: the downbeats line up too

	// Both positions move with the same audio block, so they compare as read.
	syncMultiple = applyFollow (syncFollower, in, follower.getPosition());
}

void MainComponent::setSyncSource (bool pio)
{
	// Whatever was synced stops: a deck that followed the other deck must not
	// suddenly follow a CDJ, or the other way round, without being asked.
	for (int d = 0; d < numDecks; ++d)
	{
		decks[(size_t) d]->setSyncEnabled (false);
		players[(size_t) d]->setSyncNudge (1.0);
	}
	syncFollower = -1;
	pioSynced = {};

	pioSource = pio;
	syncSourceButton.setButtonText (syncLabels::sourceButton (pio));
	appProperties.getUserSettings()->setValue ("syncSource", pio ? "pio" : "deck");

	updateNetwork();
	updatePioneerStatus();
}

// Every deck with SYNC on against the Pioneer clock.
void MainComponent::followPioneer()
{
	for (auto& event : proLink.drain())
	{
		if (const auto* beat = std::get_if<prolink::BeatPacket> (&event.packet))
			pioClock.onBeat (*beat, event.seconds);
		else if (const auto* status = std::get_if<prolink::StatusPacket> (&event.packet))
			pioClock.onStatus (*status, event.seconds);
	}

	const auto now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
	if (pioClock.bpm() <= 0.0)
		return; // nothing heard yet: nothing to follow

	for (int d = 0; d < numDecks; ++d)
	{
		if (! pioSynced[(size_t) d] || ! players[(size_t) d]->getBeatGrid().isValid())
			continue;

		FollowInput in;
		in.leaderBpm = pioClock.bpm();
		in.leaderBeatPhase = pioClock.beatPhaseAt (now);
		in.leaderPlaying = pioClock.isLive (now); // silent master: tempo held, phase left alone
		in.anyScratching = players[(size_t) d]->isScratching();
		in.multiple = pioMultiple[(size_t) d];

		// The Pioneer phase is for now; the deck's position is from its last
		// audio block. Carried forward, or the gap jitters by up to a block
		// and the nudge wobbles the pitch.
		auto& player = *players[(size_t) d];
		const auto position = positionAt (player.getPosition(), player.getPositionStamp(), now,
										  player.getEffectiveRate(), player.isPlaying() && ! player.isScratching());
		pioMultiple[(size_t) d] = applyFollow (d, in, position);
	}
}

void MainComponent::updatePioneerStatus()
{
	// PIO chosen but not listening -- the network was not up yet at start-up,
	// or the ports were busy: try again every two seconds, and say why meanwhile.
	if ((pioSource || masterDeck >= 0) && ! proLink.isRunning() && --pioRetryCountdown <= 0)
	{
		pioRetryCountdown = 120;
		updateNetwork();
	}

	const auto sending = masterDeck >= 0 && pioSender.isRunning();
	const auto masterText = sending ? juce::String (syncLabels::sendingMaster (masterDeck)) : juce::String();

	pioStatus.setVisible (pioSource || masterDeck >= 0);
	if (! pioSource)
	{
		pioPlayer.setVisible (false);
		const auto error = proLink.error();
		pioStatus.setText (! error.empty() ? juce::String (syncLabels::error (error)) : masterText, juce::dontSendNotification);
		return;
	}

	const auto now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
	const auto error = proLink.error();
	const auto noMasterInfo = proLink.isRunning() && pioClock.bpm() > 0.0 && ! pioClock.hasMaster (now);
	pioPlayer.setVisible (noMasterInfo);
	if (noMasterInfo && pioPlayer.getSelectedId() != pioClock.chosenPlayer())
		pioPlayer.setSelectedId (pioClock.chosenPlayer(), juce::dontSendNotification);

	juce::String text;
	std::string status;
	if (! error.empty())
		status = syncLabels::error (error);
	else if (pioClock.bpm() <= 0.0)
		status = syncLabels::nothingHeard();
	else if (noMasterInfo)
		status = syncLabels::noMaster();
	else
		status = syncLabels::following (pioClock.bpm(), pioClock.isLive (now) ? pioClock.leader (now) : 0);
	text = juce::String::fromUTF8 (status.c_str());

	if (sending)
		text = masterText + juce::String::fromUTF8 (" \xc2\xb7 ") + text;
	pioStatus.setText (text, juce::dontSendNotification);
}

//==============================================================================
// One question for the whole batch: the folder it goes to, below the library.
// Every track lands there, named after its file.
void MainComponent::createStems (const juce::Array<juce::File>& files)
{
	if (! stemCreator.isInstalled())
	{
		juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Create stems",
			"The stem separator (Demucs) is not installed:\n" + stemCreator.venv().getChildFile ("bin/demucs").getFullPathName()
			+ juce::String (" is missing.\n\nSet it up once (about 1 GB):\n"
							"  sudo apt install ffmpeg python3-venv\n"
							"  tools/setup-separator.sh"));
		return;
	}

	std::vector<std::string> paths;
	for (const auto& f : files)
		paths.push_back (f.getFullPathName().toStdString());
	askTargetFolder (files, juce::String (suggestTargetFolder (paths)));
}

void MainComponent::askTargetFolder (const juce::Array<juce::File>& files, const juce::String& preset)
{
	const auto count = files.size() == 1 ? files.getFirst().getFileNameWithoutExtension()
										 : juce::String (files.size()) + " tracks";
	const auto libraryFolder = library.getFolder();

	stemDialog = std::make_unique<juce::AlertWindow> ("Create stems",
		count + juce::String ("\n\nTarget folder in ") + libraryFolder.getFullPathName()
			  + juce::String::fromUTF8 (":\n(Artist/Album \xe2\x80\x93 empty: straight into the library folder)"),
		juce::MessageBoxIconType::NoIcon, this);
	stemDialog->addTextEditor ("folder", preset, "Target folder");
	stemDialog->addButton ("Create", 1, juce::KeyPress (juce::KeyPress::returnKey));
	stemDialog->addButton (juce::String::fromUTF8 ("Browse\xe2\x80\xa6"), 2);
	stemDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
	stemDialog->enterModalState (true, juce::ModalCallbackFunction::create ([this, files, libraryFolder] (int result)
	{
		const auto typed = stemDialog->getTextEditorContents ("folder").trim();
		stemDialog.reset();

		if (result == 1)
		{
			const auto folder = juce::String (sanitiseFolder (typed.toStdString()));
			for (const auto& f : files)
				stemCreator.add (f, folder, f.getFileNameWithoutExtension());
		}
		else if (result == 2)
		{
			const auto start = libraryFolder.getChildFile (juce::String (sanitiseFolder (typed.toStdString())));
			folderChooser = std::make_unique<juce::FileChooser> ("Target folder", start.isDirectory() ? start : libraryFolder);
			folderChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
				[this, files, libraryFolder, typed] (const juce::FileChooser& fc)
				{
					const auto chosen = fc.getResult();
					if (chosen == juce::File())
						return askTargetFolder (files, typed);   // closed: back to the question
					if (chosen != libraryFolder && ! chosen.isAChildOf (libraryFolder))
					{
						juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Create stems",
							juce::String ("The target folder has to be inside the library folder:\n") + libraryFolder.getFullPathName());
						return askTargetFolder (files, typed);
					}
					askTargetFolder (files, chosen == libraryFolder ? juce::String() : chosen.getRelativePathFrom (libraryFolder));
				});
		}
	}));
}

//==============================================================================
Session MainComponent::gatherSession() const
{
	Session session;
	for (int d = 0; d < numDecks; ++d)
	{
		auto& deck = session.decks[(size_t) d];
		const auto& player = *players[(size_t) d];
		deck.setId = library.storedId (loadedSetIds[(size_t) d]);
		deck.position = player.getPosition();
		deck.playing = player.isPlaying();
		deck.cuePoint = player.getCuePoint();
		deck.loop = player.hasLoop() ? player.getLoop() : juce::Range<double>();
		decks[(size_t) d]->saveState (deck);
		mixer.strip (d).saveState (deck);
	}
	session.masterDeck = masterDeck;
	session.masterTurnedOff = masterTurnedOff;
	session.autoDj = autoDj.isEnabled();
	library.saveState (session.library);
	for (const auto& job : stemCreator.pendingJobs())
		session.stemJobs.push_back ({ juce::String (job.input), juce::String (job.folder), juce::String (job.track) });
	return session;
}

void MainComponent::editGrid (int deckIndex, DeckPanel::GridAction action, double seconds)
{
	auto& player = *players[(size_t) deckIndex];
	const auto& set = loadedSets[(size_t) deckIndex];
	const auto current = player.getBeatGrid();
	if (! set || ! current.isValid())
		return;

	using Action = DeckPanel::GridAction;
	if (action == Action::reset)
	{
		analysisCache->clearCorrected (*set);
		if (const auto analysed = analysisCache->findAnalysed (*set))
		{
			player.setBeatGrid (*analysed);
			rebuildSections (deckIndex);
		}
		return;
	}

	GridEdit::Grid grid { current.bpm, current.firstBeat };
	switch (action)
	{
		case Action::shift:       grid = GridEdit::shift (grid, seconds); break;
		case Action::halfBack:    grid = GridEdit::shiftHalfBeat (grid, false); break;
		case Action::halfForward: grid = GridEdit::shiftHalfBeat (grid, true); break;
		case Action::oneBack:     grid = GridEdit::moveOne (grid, false); break;
		case Action::oneForward:  grid = GridEdit::moveOne (grid, true); break;
		case Action::snapToCue:   grid = GridEdit::snapToCue (grid, player.getCuePoint()); break;
		case Action::downbeatAtPlayhead:
		{
			// Where the playhead is now, carried from the last audio block.
			const auto now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
			grid = GridEdit::snapToCue (grid, positionAt (player.getPosition(), player.getPositionStamp(), now,
														  player.getEffectiveRate(), player.isPlaying() && ! player.isScratching()));
			break;
		}
		case Action::shiftToLeader:
		{
			// The leader: the Pioneer master under SYNC: PIO, else the other deck.
			const auto now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
			const auto position = positionAt (player.getPosition(), player.getPositionStamp(), now,
											  player.getEffectiveRate(), player.isPlaying() && ! player.isScratching());
			if (pioSource)
			{
				if (pioClock.bpm() <= 0.0)
					return;
				grid = GridEdit::shiftToPhase (grid, position, pioClock.beatPhaseAt (now));
			}
			else
			{
				const auto& leader = *players[(size_t) (1 - deckIndex)];
				const auto leaderGrid = leader.getBeatGrid();
				if (! leaderGrid.isValid() || ! leader.isPlaying())
					return;
				const auto leaderPosition = positionAt (leader.getPosition(), leader.getPositionStamp(), now,
														leader.getEffectiveRate(), ! leader.isScratching());
				const auto multiple = syncMultiple > 0.0 ? syncMultiple : 1.0;
				grid = GridEdit::shiftToPhase (grid, position, leaderGrid.beatsAt (leaderPosition) * multiple);
			}
			break;
		}
		case Action::reset: break;
	}

	const BeatGrid edited { grid.bpm, grid.firstBeat };
	player.setBeatGrid (edited);
	analysisCache->storeCorrected (*set, edited);

	// The bars follow the downbeat at once: the next preview says so (Preview.h).
	rebuildSections (deckIndex);

	// SHIFT GRID ends a bend: the grid now says what the ear did, the beat follows again.
	if (action == Action::shiftToLeader)
		setSyncBent (deckIndex, false);
}

void MainComponent::pressMaster (int deckIndex)
{
	const auto chosen = chooseMaster (deckIndex, decksPlaying(), masterDeck, true);
	masterTurnedOff = chosen < 0;
	setMasterDeck (chosen);
}

//==============================================================================
void MainComponent::scanControllers()
{
	const auto found = Scs3dDevice::find();

	bool same = found.size() == controllers.size();
	for (size_t i = 0; same && i < found.size(); ++i)
		same = controllers[i]->getIdentifier() == found[i].first.identifier;
	if (same)
		return;

	controllers.clear();
	const auto swap = settings().getBoolValue ("scs3dSwap");
	for (size_t i = 0; i < std::min<size_t> (found.size(), numDecks); ++i)
	{
		auto device = std::make_unique<Scs3dDevice> (found[i].first, found[i].second);
		if (! device->isOpen())
			continue;
		const auto deck = swap ? numDecks - 1 - (int) i : (int) i;
		device->onEvent = [this, deck] (const scs3d::Event& e) { handleController (deck, e); };
		controllers.push_back (std::move (device));
	}
}

void MainComponent::handleController (int d, const scs3d::Event& e)
{
	auto& player = *players[(size_t) d];
	auto& deck = *decks[(size_t) d];
	using Type = scs3d::Event::Type;

	switch (e.type)
	{
		case Type::mute:    mixer.strip (d).toggleMute (e.stem); break;
		case Type::play:    deck.togglePlay(); break;
		case Type::cueDown: deck.cuePressed(); break;
		case Type::cueUp:   deck.cueReleased(); break;
		case Type::master:  pressMaster (d); break;
		case Type::sync:
		{
			const auto on = ! deck.isSyncEnabled();
			deck.setSyncEnabled (on);
			setSync (d, on);
			break;
		}
		case Type::gain:  mixer.strip (d).setFaderTravel (e.value); break;
		case Type::pitch: deck.moveTempo (e.value); break;

		// Loop on button 3: the first press marks in, the second loops from
		// there to here. Button 4: loop off, and on again (back to its start).
		case Type::loopInOut:
		{
			auto& in = controllerLoopIn[(size_t) d];
			if (! player.isLoaded())
				break;
			if (in >= 0.0 && player.getPosition() > in + 0.05)
			{
				player.setLoop (in, player.getPosition());
				controllerLoop[(size_t) d] = { in, player.getPosition() };
				in = -1.0;
			}
			else
			{
				in = player.getPosition();
			}
			break;
		}
		case Type::loopToggle:
			if (player.hasLoop())
			{
				controllerLoop[(size_t) d] = player.getLoop();
				player.clearLoop();
			}
			else if (! controllerLoop[(size_t) d].isEmpty())
			{
				player.setLoop (controllerLoop[(size_t) d].getStart(), controllerLoop[(size_t) d].getEnd());
			}
			break;

		// Around the circle: the library. A tap in the centre loads -- but not
		// over a deck that is playing: a stray touch must not stop the music.
		case Type::previous: library.selectRelative (-1); break;
		case Type::next:     library.selectRelative (1); break;
		case Type::load:
			if (const auto* set = library.selectedSet(); set != nullptr && ! player.isPlaying())
			{
				loadSet (*set, d);
				controllerLoopIn[(size_t) d] = -1.0;
				controllerLoop[(size_t) d] = {};
			}
			break;

		// The circle as the platter: touch holds, turning scratches, release lets go.
		case Type::scratchTouch:   if (player.isLoaded()) player.beginScratch(); break;
		case Type::scratchMove:    if (player.isScratching()) player.scratchBy (e.value * scs3d::secondsPerScratchStep); break;
		case Type::scratchRelease: player.endScratch(); break;
		case Type::none: break;
	}
}

void MainComponent::showControllers()
{
	const auto swap = settings().getBoolValue ("scs3dSwap");
	for (size_t i = 0; i < controllers.size(); ++i)
	{
		const auto d = swap ? numDecks - 1 - (int) i : (int) i;
		const auto& player = *players[(size_t) d];
		const auto& deck = *decks[(size_t) d];

		scs3d::Leds leds;
		leds.deck = d;
		for (int s = 0; s < StemSet::numStems; ++s)
			leds.muted[(size_t) s] = mixer.strip (d).isMuted (s);
		leds.looping = player.hasLoop();
		leds.loopInSet = controllerLoopIn[(size_t) d] >= 0.0;
		leds.loopStored = ! controllerLoop[(size_t) d].isEmpty();
		leds.playing = player.isPlaying();
		leds.atCue = player.isLoaded() && std::abs (player.getPosition() - player.getCuePoint()) < 0.01;
		leds.synced = deck.isSyncEnabled();
		leds.syncBent = syncBent[(size_t) d];
		leds.master = masterDeck == d;
		leds.gain = mixer.strip (d).getFaderTravel();
		leds.pitch = deck.getTempoPosition();
		leds.platter = player.getPosition() / JogWheel::secondsPerRevolution;
		controllers[i]->show (leds);
	}
}

namespace
{
	panel::Rgb toRgb (juce::Colour c) { return { c.getRed(), c.getGreen(), c.getBlue() }; }
}

void MainComponent::handlePanel (const panel::Event& e)
{
	if (e.deck < 0 || e.deck >= numDecks)
		return;
	auto& deck = *decks[(size_t) e.deck];
	auto& strip = mixer.strip (e.deck);
	using Type = panel::Event::Type;

	switch (e.type)
	{
		case Type::route:   mixer.pressBus (e.deck, e.stem, e.bus); break;
		case Type::cueDown: deck.cuePressed(); break;
		case Type::cueUp:   deck.cueReleased(); break;
		case Type::play:    deck.togglePlay(); break;
		case Type::gain:    strip.nudgeStemGain (e.stem, e.value); break;
		case Type::mute:    strip.toggleMute (e.stem); break;
		case Type::fader:   strip.setFaderTravel (e.value); break;
	}
}

void MainComponent::showPanel()
{
	if (motionPanel == nullptr)
		return;

	panel::LedState state;
	for (int d = 0; d < numDecks; ++d)
	{
		const auto& player = *players[(size_t) d];
		for (int s = 0; s < StemSet::numStems; ++s)
		{
			const auto index = (size_t) buses::stemIndex (d, s);
			state.masks[index] = player.getStemBuses (s);
			state.muted[index] = mixer.strip (d).isMuted (s);
		}
		state.playing[(size_t) d] = player.isPlaying();
		state.atCue[(size_t) d] = player.isLoaded() && ! player.isPlaying()
								  && std::abs (player.getPosition() - player.getCuePoint()) < 0.01;
	}
	for (int s = 0; s < StemSet::numStems; ++s)
		state.stemColours[(size_t) s] = toRgb (Theme::stem (s));
	state.playColour = toRgb (Theme::play);
	state.cueColour = toRgb (Theme::cue);
	for (int d = 0; d < numDecks; ++d)
		state.deckColours[(size_t) d] = toRgb (Theme::deck (d));
	motionPanel->show (state);
}

void MainComponent::toggleRecording()
{
	if (recorder.isRecording())
	{
		recorder.stop();
		recButton.setTooltip ("Aufgenommen: " + recorder.getFile().getFullPathName());
	}
	else if (const auto error = recorder.start (Recorder::defaultFolder(), jack.getSampleRate()); error.isNotEmpty())
	{
		juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Recording", error);
	}
	updateRecorder();
}

void MainComponent::updateRecorder()
{
	recMeterL.setLevel (recorder.popPeak (0));
	recMeterR.setLevel (recorder.popPeak (1));

	const auto on = recorder.isRecording();
	recButton.setToggleState (on, juce::dontSendNotification);
	if (on)
	{
		const auto seconds = (int) recorder.getSeconds();
		recButton.setButtonText (juce::String::fromUTF8 ("\xe2\x97\x8f ") + juce::String (seconds / 60) + ":" + juce::String (seconds % 60).paddedLeft ('0', 2)
								 + (recorder.hasDropped() ? " !" : ""));
		recButton.setTooltip (recorder.getFile().getFullPathName()
							  + (recorder.hasDropped() ? juce::String ("\n! The disk fell behind: gaps in the recording") : juce::String()));
	}
	else
	{
		recButton.setButtonText ("REC");
		if (! usingJack)
			recButton.setTooltip (juce::String ("Recording needs JACK (inputs rec_L / rec_R)"));
		else if (recButton.getTooltip().isEmpty())
			recButton.setTooltip (juce::String ("Records the JACK inputs StemDeck:rec_L / rec_R as FLAC into ")
								  + Recorder::defaultFolder().getFullPathName());
	}
}

void MainComponent::setAutoDj (bool on)
{
	autoDjButton.setToggleState (on, juce::dontSendNotification);
	autoDjButton.setButtonText ("AUTO DJ");
	autoDj.setEnabled (on);
	// While it plays nothing reaches AUX; off, the routing stays as it is.
	mixer.setSpare (on ? buses::Spare::toOff : buses::Spare::toAux);
	if (on)
		runAutoDj();
}

// Next / Prev asked for the track beside the playing one; when there is none
// after all (the library changed since), Auto DJ's own pick rather than none.
const StemSet* MainComponent::autoDjPick (AutoDj::Pick pick, int loadDeck)
{
	if (pick != AutoDj::Pick::random)
		if (const auto* set = library.neighbourInFolder (loadedSetIds[(size_t) (1 - loadDeck)], pick == AutoDj::Pick::next ? 1 : -1))
			return set;
	return library.randomVisibleSet (autoDjPlayed);
}

void MainComponent::setAutoDjFade (AutoDj::MixLength length)
{
	autoDj.setMixLength (length);
	const auto applied = autoDj.mixLength();
	// The fade's bar count was written on every start whether chosen or not,
	// so it says nothing about a choice: the overlap starts from its default.
	settings().removeValue ("autoDjMixBars");
	settings().setValue ("autoDjOverlapBars", applied.bars);
	settings().setValue ("autoDjMixSeconds", applied.noGridSeconds);
	const auto overlap = applied.bars == AutoDj::aboutTwentySeconds ? juce::String ("about 20 s")
																	: juce::String (applied.bars) + " bars";
	autoDjButton.setTooltip ("Auto DJ: plays at random from the library as searched/filtered and hands each track to the "
							 "next stem by stem on the beat, over " + overlap + " (" + juce::String (applied.noGridSeconds, 0)
							 + " s without a beat grid)");
}

void MainComponent::readLevels (int deckIndex)
{
	const auto d = (size_t) deckIndex;
	const auto generation = loadGeneration[d];
	if (levelsGeneration[d] == generation || ! loadedSets[d])
		return;
	levelsGeneration[d] = generation;
	deckLevels[d].reset();
	deckSpans[d].reset();

	juce::Component::SafePointer<MainComponent> safeThis (this);
	analysisPool.addJob ([this, safeThis, set = *loadedSets[d], deckIndex, generation]
	{
		auto* job = juce::ThreadPoolJob::getCurrentThreadPoolJob();
		auto levels = StemLevels::read (set, formatManager, [job] { return job != nullptr && job->shouldExit(); });
		const auto span = levels != nullptr ? StemHandover::audibleSpan (*levels) : std::nullopt;
		juce::MessageManager::callAsync ([safeThis, deckIndex, generation, levels, span]
		{
			if (safeThis == nullptr || safeThis->loadGeneration[(size_t) deckIndex] != generation)
				return;
			safeThis->deckLevels[(size_t) deckIndex] = levels;
			safeThis->deckSpans[(size_t) deckIndex] = span;
			safeThis->levelsRead[(size_t) deckIndex] = generation;
		});
	});
}

void MainComponent::stepDeck (int deckIndex, int direction)
{
	if (autoDj.isEnabled())
	{
		// Only the deck Auto DJ plays: the other one is where its mix goes.
		if (deckIndex == autoDj.playingDeck())
			autoDj.requestMixNow (direction > 0 ? AutoDj::Pick::next : AutoDj::Pick::previous);
		return;
	}

	if (const auto* set = library.neighbourInFolder (loadedSetIds[(size_t) deckIndex], direction))
		loadSet (*set, deckIndex);
}

void MainComponent::findSteps()
{
	for (int d = 0; d < numDecks; ++d)
	{
		const auto& id = loadedSetIds[(size_t) d];
		steps[(size_t) d] = { library.neighbourInFolder (id, -1) != nullptr, library.neighbourInFolder (id, 1) != nullptr };
	}
	showSteps();
}

void MainComponent::showSteps()
{
	for (int d = 0; d < numDecks; ++d)
	{
		auto previous = steps[(size_t) d].previous;
		auto next = steps[(size_t) d].next;
		juce::String previousTip ("Previous track in this folder"), nextTip ("Next track in this folder");

		if (autoDj.isEnabled())
		{
			const auto takes = d == autoDj.playingDeck() && autoDj.canMixNow();
			previous = previous && takes;
			next = next && takes;
			previousTip = "Auto DJ: mix to the previous track in this folder now";
			nextTip = "Auto DJ: mix to the next track in this folder now";
		}
		decks[(size_t) d]->setStepsAvailable (previous, next, previousTip, nextTip);
	}
}

void MainComponent::runAutoDj()
{
	if (! autoDj.isEnabled())
		return;

	std::array<AutoDj::DeckView, numDecks> views;
	for (int d = 0; d < numDecks; ++d)
	{
		readLevels (d);
		const auto& player = *players[(size_t) d];
		auto& view = views[(size_t) d];
		const auto grid = player.getBeatGrid();
		view.loaded = player.isLoaded();
		view.playing = player.isPlaying();
		view.position = player.getPosition();
		view.length = player.getLength();
		view.gridBpm = grid.isValid() ? grid.bpm : 0.0;
		view.firstBeat = grid.firstBeat;
		view.rate = player.getEffectiveRate();
		view.looping = player.hasLoop();
		view.levels = deckLevels[(size_t) d].get();
		if (const auto& span = deckSpans[(size_t) d])
		{
			view.audibleStart = span->start;
			view.audibleEnd = span->end;
		}
		for (int s = 0; s < AutoDj::numStems; ++s)
		{
			const auto mask = player.getStemBuses (s);
			view.bus[(size_t) s] = mask == buses::offMask ? AutoDj::offBus
								 : (mask & (1u << buses::aux)) != 0 ? AutoDj::auxBus
								 : (int) juce::findHighestSetBit (mask);
		}
		view.levelsPending = levelsGeneration[(size_t) d] == loadGeneration[(size_t) d]
						  && levelsRead[(size_t) d] != loadGeneration[(size_t) d];
	}

	const auto c = autoDj.update (views);
	autoDjButton.setButtonText (autoDj.phase() == AutoDj::Phase::mixing
									? "MIX " + juce::String (juce::roundToInt (autoDj.mixProgress() * 100.0)) + " %"
									: juce::String ("AUTO DJ"));

	if (c.load >= 0)
	{
		if (const auto* set = autoDjPick (c.loadPick, c.load))
		{
			if (autoDjPlayed.count (set->files[0].getFullPathName()) > 0)
				autoDjPlayed.clear();   // every one was played: round again
			autoDjPlayed.insert (set->files[0].getFullPathName());
			loadSet (*set, c.load);
		}
		else
		{
			setAutoDj (false);   // nothing to play: the search shows no sets
			return;
		}
	}

	std::vector<buses::Route> routes;
	for (int d = 0; d < numDecks; ++d)
	{
		if (const auto db = c.faderDb[(size_t) d])
			mixer.strip (d).setFaderDb (*db);
		for (int s = 0; s < AutoDj::numStems; ++s)
			if (const auto bus = c.bus[(size_t) d][(size_t) s])
				routes.push_back ({ buses::stemIndex (d, s), *bus == AutoDj::offBus ? buses::off : *bus });
	}
	// All of a tick's switches at once, before the start, and to Core as a
	// click's are: the desk sees every stem it moved.
	if (! routes.empty())
		mixer.route (routes);

	if (c.syncOn >= 0)
	{
		decks[(size_t) c.syncOn]->setSyncEnabled (true);
		setSync (c.syncOn, true);
	}

	if (c.start >= 0)
	{
		auto& player = *players[(size_t) c.start];
		player.setPosition (c.startAt);
		player.play();
	}

	if (c.stop >= 0)
		players[(size_t) c.stop]->pause();

	if (c.syncOff >= 0)
	{
		decks[(size_t) c.syncOff]->setSyncEnabled (false);
		setSync (c.syncOff, false);
	}
}

void MainComponent::saveSession()
{
	analysisCache->flush();   // grid corrections, written with the session
	// Nothing playing and nothing touched: no write.
	const auto text = gatherSession().toXml()->toString();
	if (text == lastSessionText)
		return;
	if (Session::file (settings().getFile()).replaceWithText (text))
		lastSessionText = text;
}

void MainComponent::restoreSession()
{
	const auto session = Session::load (Session::file (settings().getFile()));
	if (! session)
		return;

	library.restoreState (session->library);

	for (int d = 0; d < numDecks; ++d)
	{
		const auto& deck = session->decks[(size_t) d];
		auto& player = *players[(size_t) d];

		mixer.strip (d).restoreState (deck);

		if (deck.setId.isEmpty())
			continue;
		if (const auto* set = library.findSet (library.idFromStored (deck.setId)))
			loadSet (*set, d);
		if (! player.isLoaded())
			continue;

		decks[(size_t) d]->restoreState (deck);
		player.setCuePoint (deck.cuePoint);
		if (! deck.loop.isEmpty())
			player.setLoop (deck.loop.getStart(), deck.loop.getEnd());
		player.setPosition (deck.position);
	}
	// Both decks restored as stored: an older session may break the
	// one-stem-per-bus rule, which spans the two decks.
	mixer.normaliseBuses();

	for (int d = 0; d < numDecks; ++d)
		if (session->decks[(size_t) d].sync && players[(size_t) d]->isLoaded())
			setSync (d, true);

	masterTurnedOff = session->masterTurnedOff;
	if (session->masterDeck >= 0 && players[(size_t) session->masterDeck]->isLoaded())
		setMasterDeck (session->masterDeck);

	// Last: what was playing plays on.
	for (int d = 0; d < numDecks; ++d)
		if (session->decks[(size_t) d].playing && players[(size_t) d]->isLoaded())
			players[(size_t) d]->play();

	if (session->autoDj)
		setAutoDj (true);   // takes the playing deck as it is

	for (const auto& job : session->stemJobs)
		if (juce::File (job.input).existsAsFile())
			stemCreator.add (juce::File (job.input), job.folder, job.track);

	lastSessionText = session->toXml()->toString();
}

void MainComponent::updateCreatorStatus()
{
	// No pause while a deck plays: the separator keeps off the audio CPU.
	const auto status = stemCreator.status();
	juce::String text;
	if (status.running)
	{
		text = "Stems: " + status.track + "  " + juce::String (juce::roundToInt (status.progress * 100.0)) + " %";
		if (status.waiting > 0)
			text << "  +" << status.waiting << " waiting";
	}
	else if (status.lastError.isNotEmpty())
		text = "Stems: failed: " + status.lastError;

	library.setCreatorStatus (text, status.running, ! status.running && status.lastError.isNotEmpty());
}

std::array<bool, 2> MainComponent::decksPlaying() const
{
	return { players[0]->isPlaying(), players[1]->isPlaying() };
}

void MainComponent::setMasterDeck (int deckIndex)
{
	masterDeck = deckIndex;
	for (int d = 0; d < numDecks; ++d)
		decks[(size_t) d]->setMaster (d == deckIndex);
	pioSender.setMaster (deckIndex >= 0 ? players[(size_t) deckIndex] : nullptr);
	updateNetwork();
}

// On the Pioneer network while following (PIO) or sending (a master deck):
// the receiver announces the virtual CDJ and listens, the sender sends the
// master deck's beat. Off the network when neither.
void MainComponent::updateNetwork()
{
	const auto device = appProperties.getUserSettings()->getIntValue ("pioDevice", 6);

	if (! pioSource && masterDeck < 0)
	{
		pioSender.stop();
		proLink.stop();
		return;
	}

	if (! proLink.isRunning())
		proLink.start (device, proLinkPorts);

	if (masterDeck >= 0 && proLink.isRunning() && ! pioSender.isRunning())
		pioSender.start (device, proLink.broadcastAddress(), proLinkPorts);
	else if (masterDeck < 0)
		pioSender.stop();
}

// One synced deck against its leader: the rules in followLeader(), the
// result put on the deck.
double MainComponent::applyFollow (int deckIndex, FollowInput in, double followerPosition)
{
	auto& follower = *players[(size_t) deckIndex];
	const auto grid = follower.getBeatGrid();

	if (std::abs (follower.getPitchBend() - 1.0) > 1e-4 && ! syncBent[(size_t) deckIndex])
		setSyncBent (deckIndex, true);
	if (syncBent[(size_t) deckIndex])
		in.leaderPlaying = false;   // the tempo only: the beat is the hand's

	in.followerGridBpm = grid.bpm;
	in.followerBeatPhase = grid.beatsAt (followerPosition);
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

	for (int ch = 0; ch < outputs::channelCount(); ++ch)
		portNames.add (juce::String (outputs::portName (ch)));

	usingJack = jack.open ("StemDeck", portNames, *this, { "rec_L", "rec_R" }, &recorder).isEmpty();
	recButton.setEnabled (usingJack);

	if (! usingJack)
		initialiseDeviceManager();

	updateDeviceStatus();
}

void MainComponent::initialiseDeviceManager()
{
	const auto numOutputs = outputs::channelCount();
	const auto savedState = appProperties.getUserSettings()->getXmlValue ("audioDeviceState");

	// Also the way back in when the output mode changed: nothing doubled up.
	deviceManager.removeChangeListener (this);
	deviceManager.removeAudioCallback (&audioSourcePlayer);
	deviceManager.closeAudioDevice();

	deviceManager.initialise (0, numOutputs, savedState.get(), true);

	// First start: enable as many of the outputs as the device has.
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
				 << jack.getNumConnectedPorts() << " connected  |  Xruns: " << jack.getXrunCount();
		else
			text = "The JACK server has stopped - please restart StemDeck";

		deviceStatus.setText (text, juce::dontSendNotification);
		deviceStatus.setColour (juce::Label::textColourId, running ? Theme::textDim : Theme::mute);
		return;
	}

	auto* device = deviceManager.getCurrentAudioDevice();

	if (device == nullptr)
	{
		deviceStatus.setText (juce::String ("No audio device open"), juce::dontSendNotification);
		deviceStatus.setColour (juce::Label::textColourId, Theme::mute);
		return;
	}

	const auto activeOutputs = device->getActiveOutputChannels().countNumberOfSetBits();
	const auto wanted = outputs::channelCount();
	auto text = deviceManager.getCurrentAudioDeviceType() + ": " + device->getName()
			  + "  |  " + juce::String (device->getCurrentSampleRate(), 0) + " Hz, "
			  + juce::String (device->getCurrentBufferSizeSamples()) + " Samples  |  "
			  + juce::String (activeOutputs) + juce::String (" outputs");

	if (activeOutputs < wanted)
		text << "  (summed down)";

	deviceStatus.setText (text, juce::dontSendNotification);
	deviceStatus.setColour (juce::Label::textColourId, activeOutputs < wanted ? Theme::cue : Theme::textDim);
}

void MainComponent::showAudioSettings()
{
	juce::DialogWindow::LaunchOptions dialog;
	dialog.content.setOwned (new AudioPanel (deviceManager, outputs::channelCount()));
	dialog.dialogTitle = "Audio";
	dialog.dialogBackgroundColour = Theme::panel;
	dialog.useNativeTitleBar = true;
	dialog.resizable = true;
	dialog.launchAsync();
}

void MainComponent::goToWorkspace (int number)
{
	juce::ChildProcess i3;
	if (i3.start (juce::StringArray { "i3-msg", "-q", "workspace number " + juce::String (number) }))
		i3.waitForProcessToFinish (1000);
}

void MainComponent::showWorkspaces()
{
	juce::ChildProcess i3;
	if (! i3.start (juce::StringArray { "i3-msg", "-t", "get_workspaces" }))
		return;

	// Held in a named var: getArray() points into it, and a temporary's list
	// was gone before the loop read it (SIGSEGV on the rig, 2026-09-30).
	const auto workspaces = juce::JSON::parse (i3.readAllProcessOutput());
	const auto* list = workspaces.getArray();
	if (list == nullptr)
		return;

	std::vector<WorkspacePanel::Entry> entries;
	for (const auto& workspace : *list)
	{
		const auto name = workspace["name"].toString().toStdString();
		if (const auto number = workspaceNumber (name); number > 0)
			entries.push_back ({ number, juce::String (workspaceLabel (name)), (bool) workspace["focused"] });
	}

	if (entries.empty())
		return;
	workspacePanel.setBounds (getLocalBounds());
	workspacePanel.show (entries);
}

void MainComponent::showTips (bool shown)
{
	if (shown && tooltips == nullptr)
		tooltips = std::make_unique<juce::TooltipWindow> (this, 600);
	else if (! shown)
		tooltips.reset();
}

void MainComponent::showSettings()
{
	juce::DialogWindow::LaunchOptions dialog;
	SettingsPanel::Values values;
	values.libraryFolder = library.getFolder();
	values.autoDjFade = autoDj.mixLength();
	values.audioDeviceChoosable = ! usingJack;
	values.audioDeviceNote = usingJack ? "Under JACK: route with qjackctl or a patchbay" : juce::String();
	values.tipsShown = tooltips != nullptr;

	SettingsPanel::Actions actions;
	actions.onLibraryFolder = [this] (const juce::File& folder) { library.setFolder (folder); };
	actions.onAutoDjFade = [this] (AutoDj::MixLength length) { setAutoDjFade (length); };
	actions.onAudioDevice = [this] { showAudioSettings(); };
	actions.onTips = [this] (bool shown)
	{
		settings().setValue (tips::settingKey, juce::String (tips::stored (shown)));
		showTips (shown);
	};

	dialog.content.setOwned (new SettingsPanel (values, std::move (actions)));
	dialog.dialogTitle = "Settings";
	dialog.dialogBackgroundColour = Theme::panel;
	dialog.useNativeTitleBar = true;
	dialog.resizable = false;
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
	sendPreview();

	// With no master yet, the only playing deck becomes it (like a lone CDJ).
	if (const auto chosen = chooseMaster (-1, decksPlaying(), masterDeck, ! pioSource && ! masterTurnedOff); chosen != masterDeck)
		setMasterDeck (chosen);

	updatePioneerStatus();
	updateCreatorStatus();

	runAutoDj();
	showSteps();
	updateRecorder();

	if (--controllerScanCountdown <= 0)
	{
		controllerScanCountdown = 180;   // three seconds
		scanControllers();
	}
	showControllers();
	showPanel();

	if (--sessionCountdown <= 0)
	{
		sessionCountdown = 120;   // two seconds at 60 Hz
		saveSession();
	}

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
	busBuffer.setSize (outputs::channelCount(), maxBlock);

	for (int d = 0; d < numDecks; ++d)
	{
		deckBuffers[(size_t) d].setSize (StemDeckPlayer::numOutputChannels, maxBlock);
		players[(size_t) d]->prepareToPlay (samplesPerBlockExpected, sampleRate);

		const auto& player = *players[(size_t) d];
		for (int s = 0; s < StemSet::numStems; ++s)
			for (int bus = 0; bus < buses::count; ++bus)
			{
				auto& route = routeGains[(size_t) d][(size_t) s][(size_t) bus];
				route.reset (sampleRate, 0.01);
				route.setCurrentAndTargetValue (buses::gain (player.isStemOnBus (s, bus), player.getDeckGain()));
			}
	}
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
	// The audio thread gets the CPU the stem separator stays off (StemJob.h),
	// so a deck plays on while a track is separated. Once per thread: JACK
	// and the ALSA fallback each have their own.
	static thread_local const bool pinned = [] {
		cpu_set_t cpus;
		CPU_ZERO (&cpus);
		CPU_SET (audioCpu, &cpus);
		return pthread_setaffinity_np (pthread_self(), sizeof (cpus), &cpus) == 0;
	}();
	juce::ignoreUnused (pinned);

	auto& out = *bufferToFill.buffer;
	const auto numSamples = bufferToFill.numSamples;
	const auto numOut = out.getNumChannels();

	const auto numBusChannels = outputs::channelCount();

	if (numSamples > busBuffer.getNumSamples())
		busBuffer.setSize (numBusChannels, numSamples, false, false, true);

	busBuffer.clear (0, numSamples);

	for (int d = 0; d < numDecks; ++d)
	{
		auto& deckBuffer = deckBuffers[(size_t) d];
		auto& player = *players[(size_t) d];

		if (numSamples > deckBuffer.getNumSamples())
			deckBuffer.setSize (StemDeckPlayer::numOutputChannels, numSamples, false, false, true);

		player.getNextAudioBlock (juce::AudioSourceChannelInfo (&deckBuffer, 0, numSamples));

		// Each stem onto every bus it is switched to, post fader; ramped over
		// 10 ms on any change.
		const auto fader = player.getDeckGain();
		for (int s = 0; s < StemSet::numStems; ++s)
			for (int bus = 0; bus < buses::count; ++bus)
			{
				auto& route = routeGains[(size_t) d][(size_t) s][(size_t) bus];
				route.setTargetValue (buses::gain (player.isStemOnBus (s, bus), fader));
				const auto from = route.getCurrentValue();
				route.skip (numSamples);
				const auto to = route.getCurrentValue();
				if (from == 0.0f && to == 0.0f)
					continue;

				for (int c = 0; c < 2; ++c)
					busBuffer.addFromWithRamp (bus * 2 + c, 0, deckBuffer.getReadPointer (s * 2 + c), numSamples, from, to);
			}
	}

	// The fixed 6 dB trim on every bus, before the meters, so they show what
	// leaves StemDeck.
	for (int ch = 0; ch < numBusChannels; ++ch)
		busBuffer.applyGain (ch, 0, numSamples, buses::trim);

	// The desk's SA meter: the AUX bus as it goes to the return, side by side.
	for (int side = 0; side < 2; ++side)
		auxLevels[(size_t) side].add (remote::measure (busBuffer.getReadPointer (buses::aux * 2 + side), numSamples));

	// The meters show the five buses.
	for (int ch = 0; ch < numOutputChannels; ++ch)
	{
		const auto peak = busBuffer.getMagnitude (ch, 0, numSamples);

		if (peak > outputPeaks[(size_t) ch].load())
			outputPeaks[(size_t) ch] = peak;
	}

	// One output per bus channel; with fewer outputs (e.g. plain stereo for
	// monitoring) the channels are summed down.
	bufferToFill.clearActiveBufferRegion();

	if (numOut > 0)
		for (int ch = 0; ch < numBusChannels; ++ch)
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
	// The band: one tile per deck (the meters' tile paints itself).
	for (const auto& tile : deckTiles)
	{
		g.setColour (Theme::panel);
		g.fillRoundedRectangle (tile.toFloat(), Theme::corner (tile.toFloat()));
	}

	// Each deck's line, from its column's stripe (the column paints that)
	// down, along the gap above the band and down between the deck's tile
	// and the meters' tile; every tile one gap from it.
	for (size_t d = 0; d < (size_t) numDecks; ++d)
	{
		g.setColour (Theme::deck ((int) d));
		for (const auto& piece : { deckLines[d].drop, deckLines[d].top, deckLines[d].side })
			g.fillRect (surface::toJuce (piece));
	}
}

void MainComponent::resized()
{
	// The sections (SurfaceLayout.h): the top bar; the waveforms with a deck's
	// pitch column at each outer edge; deck A | mixer | deck B; the library.
	const auto layout = surface::sections (getWidth(), getHeight());

	auto topBar = surface::toJuce (layout.topBar);
	// The switch at the very right, where A3 Motion has it: the key under the
	// finger stays put when the workspace changes.
	const auto switcher = switcherGeometry (getWidth());
	auto switchArea = topBar.withRight (getWidth() - switcher.margin);
	workspacesButton.setBounds (switchArea.removeFromRight (switcher.arrowWidth));
	switchArea.removeFromRight (switcher.gap);
	motionButton.setBounds (switchArea.removeFromRight (switcher.appKeyWidth));
	// The rest as shares of the window's width (sized for 768 px, the rig).
	const auto share = [this] (int perMille) { return getWidth() * perMille / 1000; };
	const auto gap = layout.metrics.gap;
	topBar.setRight (motionButton.getX() - gap);
	settingsButton.setBounds (topBar.removeFromRight (share (117)));
	topBar.removeFromRight (gap);
	keysButton.setBounds (topBar.removeFromRight (share (78)));
	topBar.removeFromRight (gap);
	syncSourceButton.setBounds (topBar.removeFromRight (share (143)));
	topBar.removeFromRight (gap);
	autoDjButton.setBounds (topBar.removeFromRight (share (130)));
	topBar.removeFromRight (gap);
	// The input meters left of REC, where the eye comes from.
	recButton.setBounds (topBar.removeFromRight (share (130)));
	topBar.removeFromRight (gap * 2 / 3);
	const auto recMeterInset = topBar.getHeight() / 10;
	recMeterR.setBounds (topBar.removeFromRight (share (8)).reduced (0, recMeterInset));
	topBar.removeFromRight (gap / 3);
	recMeterL.setBounds (topBar.removeFromRight (share (8)).reduced (0, recMeterInset));
	topBar.removeFromRight (gap);
	pioPlayer.setBounds (topBar.removeFromRight (share (117)));
	pioStatus.setBounds (topBar.removeFromRight (share (286)));
	deviceStatus.setBounds (topBar);

	waveA.setBounds (surface::toJuce (layout.waveA));
	waveB.setBounds (surface::toJuce (layout.waveB));
	for (int d = 0; d < numDecks; ++d)
		decks[(size_t) d]->setBounds (surface::toJuce (layout.deck[(size_t) d]));
	mixer.setBounds (surface::toJuce (layout.mixer));
	library.setBounds (surface::toJuce (layout.library));

	// The band under the decks and the mixer: their parts, placed here.
	const auto band = surface::band (layout.band, layout.metrics);
	for (size_t d = 0; d < (size_t) numDecks; ++d)
	{
		decks[d]->setBandBounds (band.deck[d]);
		deckTiles[d] = surface::toJuce (band.deck[d].tile);
		deckLines[d] = surface::deckLine (layout, band, (int) d);
	}
	mixer.setBandBounds (surface::toJuce (band.deck[0].fader), surface::toJuce (band.deck[1].fader),
						 surface::toJuce (band.metersTile), layout.metrics.gap);
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
