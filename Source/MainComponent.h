#pragma once

#include <JuceHeader.h>
#include "StemDeckPlayer.h"
#include "StemThumbnails.h"
#include "StemLibrary.h"
#include "SettingsPanel.h"
#include "Outputs.h"
#include "Workspaces.h"
#include "WorkspacePanel.h"
#include "DeckPanel.h"
#include "MixerPanel.h"
#include "RemoteLink.h"
#include "TruthKeeperLink.h"
#include "Buses.h"
#include "Session.h"
#include "AutoDj.h"
#include "Recorder.h"
#include "Scs3dDevice.h"
#include "Waveforms.h"
#include "JackOutput.h"
#include "Theme.h"
#include "TempoAnalysis.h"
#include "FollowLeader.h"
#include "PioneerClock.h"
#include "OscTruth.h"
#include "ProLinkReceiver.h"
#include "ProLinkSender.h"
#include "MasterDeck.h"
#include "StemCreator.h"

//==============================================================================
// Two stem decks and a mixer, laid out like Mixxx: scrolling waveforms on top,
// deck A | mixer | deck B in the middle, library at the bottom.
//
// Output is 12 channels, six stereo buses (Buses.h): 1-4 and AUX post fader,
// PHONES pre fader. Each stem of each deck is on any of them by its bus
// switches (a new set: stem N on bus N). With a JACK server running these
// are 12 ports (deck1_L ... deck4_R, aux_L/R, phones_L/R), otherwise a
// regular audio device is used.
class MainComponent  : public juce::Component,
					   public juce::AudioSource,
					   public juce::DragAndDropContainer,
					   private juce::ChangeListener,
					   private juce::Timer
{
public:
	MainComponent();
	~MainComponent() override;

	// The window's place and size, kept with the other settings.
	juce::PropertiesFile& settings() { return *appProperties.getUserSettings(); }

	//==============================================================================
	void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
	void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
	void releaseResources() override;

	//==============================================================================
	void paint (juce::Graphics& g) override;
	void resized() override;
	bool keyPressed (const juce::KeyPress& key) override;
	bool keyStateChanged (bool isKeyDown) override;

private:
	static constexpr int numDecks = 2;
	static constexpr int numBuses = OutputMeters::numBuses;
	static constexpr int numOutputChannels = OutputMeters::numChannels;

	void loadSet (const StemSet& set, int deckIndex);
	void startAnalysis (const StemSet& set, int deckIndex);
	void setSync (int deckIndex, bool enabled);
	void updateSync();
	double applyFollow (int deckIndex, FollowInput in, double followerPosition);
	void setSyncSource (bool pio);
	void setMasterDeck (int deckIndex);
	void updateNetwork();
	std::array<bool, 2> decksPlaying() const;
	void followPioneer();
	void updatePioneerStatus();
	void loadDroppedSet (const juce::String& setId, int deckIndex);
	void initialiseAudio();
	void initialiseDeviceManager();
	void changeListenerCallback (juce::ChangeBroadcaster*) override;
	void timerCallback() override;
	void createStems (const juce::Array<juce::File>& files);
	void askTargetFolder (const juce::Array<juce::File>& files, const juce::String& preset);
	void updateCreatorStatus();

	// The session (Session.h): gathered from the decks, the mixer, the library
	// and the stem creator; written when it changed, every two seconds and on
	// quit; brought back once at start.
	Session gatherSession() const;
	void saveSession();
	void restoreSession();
	juce::String lastSessionText;
	int sessionCountdown = 0;
	void updateDeviceStatus();
	void showAudioSettings();
	void showSettings();

	// Over to A3 Motion in one tap, or to any of the rig's i3 workspaces from
	// the list beside it -- named in a3-core's i3 config, read from i3.
	void goToWorkspace (int number);
	void showWorkspaces();

	DJLookAndFeel lookAndFeel;
	juce::TooltipWindow tooltips { this, 600 };
	juce::ApplicationProperties appProperties;

	juce::AudioFormatManager formatManager;
	juce::AudioThumbnailCache thumbCache { 16 };

	StemDeckPlayer playerA { formatManager }, playerB { formatManager };
	std::array<StemDeckPlayer*, numDecks> players { &playerA, &playerB };
	std::array<juce::AudioBuffer<float>, numDecks> deckBuffers; // 8 channels each (stem pairs)
	juce::AudioBuffer<float> busBuffer;                            // 12 channels (6 stereo buses) or 16 (stems)

	// Read on the audio thread; only changed while the output is closed
	// (setOutputMode), so a block never sees a mode its buffers aren't sized for.

	// Stem -> bus gains (switch x fader, Buses.h); ramped so switching and
	// fader moves don't click.
	std::array<std::array<std::array<juce::SmoothedValue<float>, buses::count>, StemSet::numStems>, numDecks> routeGains;
	std::array<std::atomic<float>, numOutputChannels> outputPeaks {};

	StemThumbnails thumbsA { formatManager, thumbCache }, thumbsB { formatManager, thumbCache };
	ScrollingWaveform waveA { playerA, thumbsA, 0 }, waveB { playerB, thumbsB, 1 };
	DeckPanel deckA { playerA, thumbsA, 0 }, deckB { playerB, thumbsB, 1 };
	MixerPanel mixer { playerA, playerB };
	// The desk's remote control through Core (spec stemdeck-remote).
	RemoteLink remote { { &playerA, &playerB }, mixer };
	// The truth from Core (spec truth-from-core, step 3).
	std::unique_ptr<TruthKeeperLink> truthKeeper;
	std::string truthPath;
	juce::String truthHash;
	StemLibrary library { formatManager };
	StemCreator stemCreator;
	std::unique_ptr<juce::AlertWindow> stemDialog;
	std::unique_ptr<juce::FileChooser> folderChooser;

	std::array<StemThumbnails*, numDecks> thumbs { &thumbsA, &thumbsB };
	std::array<ScrollingWaveform*, numDecks> waves { &waveA, &waveB };
	std::array<DeckPanel*, numDecks> decks { &deckA, &deckB };

	juce::TextButton audioSettingsButton { "Audio" };
	juce::TextButton settingsButton { "Settings" };
	juce::TextButton motionButton { "MOTION" };
	juce::TextButton workspacesButton { juce::String::fromUTF8 ("\xe2\x96\xbe") };
	WorkspacePanel workspacePanel;
	juce::Label deviceStatus;
	int statusCountdown = 0;

	// Keyboard: edge-detected so held keys don't repeat.
	struct KeyBinding { int key; std::function<void (bool down)> action; bool wasDown = false; };
	std::vector<KeyBinding> keyBindings;

	// Tempo analysis runs in the background when a set is loaded; results are cached.
	std::unique_ptr<AnalysisCache> analysisCache;
	juce::ThreadPool analysisPool { juce::ThreadPoolOptions{}.withThreadName ("Tempo analysis").withNumberOfThreads (2) };
	std::array<int, numDecks> loadGeneration {}; // drops results for a set no longer loaded
	std::array<juce::String, numDecks> loadedSetIds; // StemLibrary's id: the first stem's path
	std::array<std::optional<StemSet>, numDecks> loadedSets;

	// As on a CDJ-3000: bending a synced deck with the jog ring stops it
	// following the leader's beat -- the tempo still follows -- so it can be
	// aligned by ear; SHIFT GRID puts that into the grid and the beat follows
	// again. Also ends with SYNC going off or on.
	std::array<bool, numDecks> syncBent {};
	void setSyncBent (int deckIndex, bool bent);

	// Grid Adjust on a deck (DeckPanel::GridAction): edits its grid, keeps the
	// correction in the analysis cache (written with the session).
	void editGrid (int deckIndex, DeckPanel::GridAction action, double seconds);

	// Auto-DJ (AutoDj.h): run every tick while on; picks at random from what
	// the library's search shows, each set once until all were played.
	AutoDj autoDj;
	juce::TextButton autoDjButton { "AUTO DJ" };
	std::set<juce::String> autoDjPlayed;
	void setAutoDj (bool on);
	void runAutoDj();

	// The deck with SYNC on follows the other one's tempo and beat phase.
	int syncFollower = -1;
	double syncMultiple = 1.0; // 0.5 / 1 / 2 when the tempos are an octave apart

	// SYNC source PIO (2026-09-29): every deck with SYNC on follows the Pioneer
	// tempo master instead of the other deck, each with its own octave multiple.
	bool pioSource = false;
	std::array<bool, numDecks> pioSynced {};
	std::array<double, numDecks> pioMultiple {};
	ProLinkReceiver proLink;
	// The Pro DJ Link ports, from the one truth (a3-osc.json), read once.
	osctruth::ProLinkPorts proLinkPorts;
	PioneerClock pioClock;
	juce::TextButton syncSourceButton { "SYNC: DECK" };
	juce::Label pioStatus;
	juce::ComboBox pioPlayer;
	int pioRetryCountdown = 0; // timer ticks until the next start attempt

	// StemDeck as the tempo master (Part 2): the deck whose beat goes out, -1 none.
	int masterDeck = -1;
	// The only playing deck becomes master by itself -- but not under SYNC: PIO
	// (a real CDJ may hold master) and not once MASTER was turned off by hand.
	bool masterTurnedOff = false;
	ProLinkSender pioSender;

	// Two JACK inputs (rec_L, rec_R) to FLAC: REC in the top bar, with the
	// inputs' levels beside it. Only under JACK.
	// Stanton SCS.3d controllers (Scs3d.h), one per deck: the first found
	// is A unless the setting scs3dSwap says otherwise. Looked for again every
	// few seconds, so one plugged in later is taken up.
	std::vector<std::unique_ptr<Scs3dDevice>> controllers;
	std::array<double, numDecks> controllerLoopIn { -1.0, -1.0 };       // marked, waiting for out
	std::array<juce::Range<double>, numDecks> controllerLoop;          // the last loop, to go back to
	int controllerScanCountdown = 0;
	void scanControllers();
	void handleController (int deckIndex, const scs3d::Event& event);
	void showControllers();
	void pressMaster (int deckIndex);

	Recorder recorder;
	juce::TextButton recButton { "REC" };

	LevelMeter recMeterL, recMeterR;
	void toggleRecording();
	void updateRecorder();

	JackOutput jack;
	bool usingJack = false;

	// Fallback when no JACK server is running.
	juce::AudioDeviceManager deviceManager;
	juce::AudioSourcePlayer audioSourcePlayer;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
