#include "MainComponent.h"
#include "StemJob.h"
#include "GridEdit.h"

#include <pthread.h>
#include <sched.h>

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
		decks[(size_t) d]->onGridEdit = [this, d] (DeckPanel::GridAction action, double seconds) { editGrid (d, action, seconds); };
		decks[(size_t) d]->onMasterPressed = [this, d]
		{
			const auto chosen = chooseMaster (d, decksPlaying(), masterDeck, true);
			masterTurnedOff = chosen < 0;
			setMasterDeck (chosen);
		};
	}

	addAndMakeVisible (mixer);
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

	audioSettingsButton.onClick = [this] { showAudioSettings(); };
	audioSettingsButton.setMouseClickGrabsKeyboardFocus (false);
	addAndMakeVisible (audioSettingsButton);
	deviceStatus.setColour (juce::Label::textColourId, Theme::textDim);
	addAndMakeVisible (deviceStatus);

	autoDjButton.setClickingTogglesState (true);
	autoDjButton.setMouseClickGrabsKeyboardFocus (false);
	autoDjButton.setColour (juce::TextButton::buttonOnColourId, Theme::play);
	autoDjButton.setTooltip (juce::String::fromUTF8 ("Auto-DJ: spielt zuf\xc3\xa4llig aus der Library-Auswahl (Suche/Filter) und mischt taktgenau \xc3\xbc" "ber 16 Takte"));
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
		pioPlayer.addItem ("CDJ " + juce::String (player), player);
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
}

MainComponent::~MainComponent()
{
	stopTimer();
	saveSession();
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
		juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Set konnte nicht geladen werden", error);
		return;
	}

	loadedSetIds[d] = set.files[0].getFullPathName();
	loadedSets[d] = set;
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
	syncSourceButton.setButtonText (pio ? "SYNC: PIO" : "SYNC: DECK");
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
	const auto masterText = sending ? juce::String ("PIO master: ") + (masterDeck == 0 ? "A" : "B") : juce::String();

	pioStatus.setVisible (pioSource || masterDeck >= 0);
	if (! pioSource)
	{
		pioPlayer.setVisible (false);
		const auto error = proLink.error();
		pioStatus.setText (! error.empty() ? "PIO: " + juce::String (error) : masterText, juce::dontSendNotification);
		return;
	}

	const auto now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
	const auto error = proLink.error();
	const auto noMasterInfo = proLink.isRunning() && pioClock.bpm() > 0.0 && ! pioClock.hasMaster (now);
	pioPlayer.setVisible (noMasterInfo);
	if (noMasterInfo && pioPlayer.getSelectedId() != pioClock.chosenPlayer())
		pioPlayer.setSelectedId (pioClock.chosenPlayer(), juce::dontSendNotification);

	juce::String text;
	if (! error.empty())
		text = "PIO: " + juce::String (error);
	else if (pioClock.bpm() <= 0.0)
		text = juce::String::fromUTF8 ("PIO \xe2\x80\x93");
	else if (noMasterInfo)
		text = juce::String::fromUTF8 ("PIO: no master \xc2\xb7 follow");
	else
		text = "PIO " + juce::String (pioClock.bpm(), 1) + juce::String::fromUTF8 (" \xc2\xb7 ")
			 + (pioClock.isLive (now) ? "CDJ " + juce::String (pioClock.leader (now)) : juce::String ("held"));

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
		juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Stems erstellen",
			"Der Stem-Separator (Demucs) ist nicht installiert:\n" + stemCreator.venv().getChildFile ("bin/demucs").getFullPathName()
			+ juce::String::fromUTF8 (" fehlt.\n\nEinmalig einrichten (ca. 1 GB):\n"
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
										 : juce::String (files.size()) + " Tracks";
	const auto libraryFolder = library.getFolder();

	stemDialog = std::make_unique<juce::AlertWindow> ("Stems erstellen",
		count + juce::String::fromUTF8 ("\n\nZielordner in ") + libraryFolder.getFullPathName()
			  + juce::String::fromUTF8 (":\n(Artist/Album \xe2\x80\x93 leer: direkt in den Library-Ordner)"),
		juce::MessageBoxIconType::NoIcon, this);
	stemDialog->addTextEditor ("folder", preset, "Zielordner");
	stemDialog->addButton ("Erstellen", 1, juce::KeyPress (juce::KeyPress::returnKey));
	stemDialog->addButton (juce::String::fromUTF8 ("Durchsuchen\xe2\x80\xa6"), 2);
	stemDialog->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));
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
			folderChooser = std::make_unique<juce::FileChooser> ("Zielordner", start.isDirectory() ? start : libraryFolder);
			folderChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
				[this, files, libraryFolder, typed] (const juce::FileChooser& fc)
				{
					const auto chosen = fc.getResult();
					if (chosen == juce::File())
						return askTargetFolder (files, typed);   // closed: back to the question
					if (chosen != libraryFolder && ! chosen.isAChildOf (libraryFolder))
					{
						juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Stems erstellen",
							juce::String::fromUTF8 ("Der Zielordner muss im Library-Ordner liegen:\n") + libraryFolder.getFullPathName());
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
			player.setBeatGrid (*analysed);
		return;
	}

	GridEdit::Grid grid { current.bpm, current.firstBeat };
	switch (action)
	{
		case Action::shift:       grid = GridEdit::shift (grid, seconds); break;
		case Action::halfBack:    grid = GridEdit::shiftHalfBeat (grid, false); break;
		case Action::halfForward: grid = GridEdit::shiftHalfBeat (grid, true); break;
		case Action::snapToCue:   grid = GridEdit::snapToCue (grid, player.getCuePoint()); break;
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

	// SHIFT GRID ends a bend: the grid now says what the ear did, the beat follows again.
	if (action == Action::shiftToLeader)
		setSyncBent (deckIndex, false);
}

void MainComponent::setAutoDj (bool on)
{
	autoDjButton.setToggleState (on, juce::dontSendNotification);
	autoDjButton.setButtonText ("AUTO DJ");
	autoDj.setEnabled (on);
	if (on)
		runAutoDj();
}

void MainComponent::runAutoDj()
{
	if (! autoDj.isEnabled())
		return;

	std::array<AutoDj::DeckView, numDecks> views;
	for (int d = 0; d < numDecks; ++d)
	{
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
	}

	const auto c = autoDj.update (views);
	autoDjButton.setButtonText (autoDj.phase() == AutoDj::Phase::mixing
									? "MIX " + juce::String (juce::roundToInt (autoDj.mixProgress() * 100.0)) + " %"
									: juce::String ("AUTO DJ"));

	if (c.load >= 0)
	{
		if (const auto* set = library.randomVisibleSet (autoDjPlayed))
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

	for (int d = 0; d < numDecks; ++d)
		if (const auto db = c.faderDb[(size_t) d])
			mixer.strip (d).setFaderDb (*db);

	if (c.syncOn >= 0)
	{
		decks[(size_t) c.syncOn]->setSyncEnabled (true);
		setSync (c.syncOn, true);
	}

	if (c.start >= 0)
	{
		auto& player = *players[(size_t) c.start];
		const auto grid = player.getBeatGrid();
		player.setPosition (grid.isValid() ? grid.firstBeat : 0.0);
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
			text << "  +" << status.waiting << " wartend";
	}
	else if (status.lastError.isNotEmpty())
		text = "Stems: Fehler bei " + status.lastError;

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
		proLink.start (device);

	if (masterDeck >= 0 && proLink.isRunning() && ! pioSender.isRunning())
		pioSender.start (device, proLink.broadcastAddress());
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

	for (int bus = 0; bus < numBuses; ++bus)
	{
		const auto name = juce::String (buses::portName (bus));
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

	// With no master yet, the only playing deck becomes it (like a lone CDJ).
	if (const auto chosen = chooseMaster (-1, decksPlaying(), masterDeck, ! pioSource && ! masterTurnedOff); chosen != masterDeck)
		setMasterDeck (chosen);

	updatePioneerStatus();
	updateCreatorStatus();

	runAutoDj();

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
	busBuffer.setSize (numOutputChannels, maxBlock);

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
				route.setCurrentAndTargetValue (buses::gain (bus, player.isStemOnBus (s, bus), player.isDeckPhones(), player.getDeckGain()));
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

		// Each stem onto every bus it is switched to: post fader on 1-4 and
		// AUX, pre fader on PHONES; ramped over 10 ms on any change.
		const auto fader = player.getDeckGain();
		const auto phones = player.isDeckPhones();
		for (int s = 0; s < StemSet::numStems; ++s)
			for (int bus = 0; bus < buses::count; ++bus)
			{
				auto& route = routeGains[(size_t) d][(size_t) s][(size_t) bus];
				route.setTargetValue (buses::gain (bus, player.isStemOnBus (s, bus), phones, fader));
				const auto from = route.getCurrentValue();
				route.skip (numSamples);
				const auto to = route.getCurrentValue();
				if (from == 0.0f && to == 0.0f)
					continue;

				for (int c = 0; c < 2; ++c)
					busBuffer.addFromWithRamp (bus * 2 + c, 0, deckBuffer.getReadPointer (s * 2 + c), numSamples, from, to);
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
	topBar.removeFromRight (6);
	syncSourceButton.setBounds (topBar.removeFromRight (110));
	topBar.removeFromRight (6);
	autoDjButton.setBounds (topBar.removeFromRight (100));
	pioPlayer.setBounds (topBar.removeFromRight (90));
	pioStatus.setBounds (topBar.removeFromRight (220));
	deviceStatus.setBounds (topBar);
	area.removeFromTop (4);

	const auto waveHeight = juce::jlimit (70, 130, getHeight() / 10);
	waveA.setBounds (area.removeFromTop (waveHeight));
	area.removeFromTop (3);
	waveB.setBounds (area.removeFromTop (waveHeight));
	area.removeFromTop (6);

	auto middle = area.removeFromTop (juce::jmin (460, area.getHeight() - 150));
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
