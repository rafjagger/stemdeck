#pragma once

#include <JuceHeader.h>
#include "StemDeckPlayer.h"
#include "StemThumbnails.h"
#include "StemLibrary.h"
#include "DeckPanel.h"
#include "MixerPanel.h"
#include "Buses.h"
#include "Waveforms.h"
#include "JackOutput.h"
#include "Theme.h"
#include "TempoAnalysis.h"
#include "FollowLeader.h"
#include "PioneerClock.h"
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
	void updateDeviceStatus();
	void showAudioSettings();

	DJLookAndFeel lookAndFeel;
	juce::TooltipWindow tooltips { this, 600 };
	juce::ApplicationProperties appProperties;

	juce::AudioFormatManager formatManager;
	juce::AudioThumbnailCache thumbCache { 16 };

	StemDeckPlayer playerA { formatManager }, playerB { formatManager };
	std::array<StemDeckPlayer*, numDecks> players { &playerA, &playerB };
	std::array<juce::AudioBuffer<float>, numDecks> deckBuffers; // 8 channels each (stem pairs)
	juce::AudioBuffer<float> busBuffer;                            // 12 channels (6 stereo buses)

	// Stem -> bus gains (switch x fader, Buses.h); ramped so switching and
	// fader moves don't click.
	std::array<std::array<std::array<juce::SmoothedValue<float>, buses::count>, StemSet::numStems>, numDecks> routeGains;
	std::array<std::atomic<float>, numOutputChannels> outputPeaks {};

	StemThumbnails thumbsA { formatManager, thumbCache }, thumbsB { formatManager, thumbCache };
	ScrollingWaveform waveA { playerA, thumbsA, 0 }, waveB { playerB, thumbsB, 1 };
	DeckPanel deckA { playerA, thumbsA, 0 }, deckB { playerB, thumbsB, 1 };
	MixerPanel mixer { playerA, playerB };
	StemLibrary library { formatManager };
	StemCreator stemCreator;
	std::unique_ptr<juce::AlertWindow> stemDialog;
	std::unique_ptr<juce::FileChooser> folderChooser;

	std::array<StemThumbnails*, numDecks> thumbs { &thumbsA, &thumbsB };
	std::array<ScrollingWaveform*, numDecks> waves { &waveA, &waveB };
	std::array<DeckPanel*, numDecks> decks { &deckA, &deckB };

	juce::TextButton audioSettingsButton { "Audio-Einstellungen" };
	juce::Label deviceStatus;
	int statusCountdown = 0;

	// Keyboard: edge-detected so held keys don't repeat.
	struct KeyBinding { int key; std::function<void (bool down)> action; bool wasDown = false; };
	std::vector<KeyBinding> keyBindings;

	// Tempo analysis runs in the background when a set is loaded; results are cached.
	std::unique_ptr<AnalysisCache> analysisCache;
	juce::ThreadPool analysisPool { juce::ThreadPoolOptions{}.withThreadName ("Tempo analysis").withNumberOfThreads (2) };
	std::array<int, numDecks> loadGeneration {}; // drops results for a set no longer loaded

	// The deck with SYNC on follows the other one's tempo and beat phase.
	int syncFollower = -1;
	double syncMultiple = 1.0; // 0.5 / 1 / 2 when the tempos are an octave apart

	// SYNC source PIO (2026-09-29): every deck with SYNC on follows the Pioneer
	// tempo master instead of the other deck, each with its own octave multiple.
	bool pioSource = false;
	std::array<bool, numDecks> pioSynced {};
	std::array<double, numDecks> pioMultiple {};
	ProLinkReceiver proLink;
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

	JackOutput jack;
	bool usingJack = false;

	// Fallback when no JACK server is running.
	juce::AudioDeviceManager deviceManager;
	juce::AudioSourcePlayer audioSourcePlayer;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
