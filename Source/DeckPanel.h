#pragma once

#include <JuceHeader.h>
#include "StemDeckPlayer.h"
#include "StemThumbnails.h"
#include "Waveforms.h"
#include "JogWheel.h"
#include "Session.h"

// One deck, as a column: title, times, BPM, the Grid Adjust rows, track
// search | CUE | PLAY | track search, and the keys below (2026-10-08). Its
// overview and pitch fader (with value and range key) stand in the band
// under the decks and the mixer: the deck owns and drives them, the window
// holds and places them (addBandPartsTo, setBandBounds).
//
// Cue works like Mixxx's CDJ mode: while playing it jumps back to the cue
// point and stops; while stopped it sets the cue point, or, if already on
// it, plays for as long as it is held.
class DeckPanel : public juce::Component,
				  public StemSetDropTarget
{
public:
	DeckPanel (StemDeckPlayer& player, StemThumbnails& thumbnails, int deckIndex);

	void setSet (const StemSet& set);
	void setAnalysing (bool isAnalysing) { analysing = isAnalysing; }
	void refresh(); // called by the main timer

	void togglePlay();
	void cuePressed();
	void cueReleased();

	// Tempo set by sync: widens the tempo range if needed and doesn't count
	// as the user moving the fader.
	void setTempoFromSync (double rate);
	void setSyncEnabled (bool enabled) { syncButton.setToggleState (enabled, juce::dontSendNotification); }

	// A controller's relative pitch slider: `steps` of 1/128 of the range's
	// full travel, moved as by hand (so it takes the deck out of sync).
	void moveTempo (double steps) { pitchFader.setValue (pitchFader.getValue() + steps * 2.0 * tempoRange / 128.0, juce::sendNotificationSync); }
	// -1 .. 1: where the tempo is in its range.
	double getTempoPosition() const { return (pitchFader.getValue() - 1.0) / tempoRange; }
	bool isSyncEnabled() const { return syncButton.getToggleState(); }
	// SYNC on but only the tempo followed: bent by hand (CDJ-3000), until SHIFT.
	void setSyncBpmOnly (bool bpmOnly) { syncButton.setButtonText (bpmOnly ? "SYNC BPM" : "SYNC"); }

	std::function<void (bool enabled)> onSyncToggled;

	// Previous / next track in the loaded track's folder (MainComponent says
	// which way there is one, and what a press does under Auto DJ).
	std::function<void (int direction)> onStep;
	void setStepsAvailable (bool previous, bool next, const juce::String& previousTip, const juce::String& nextTip);

	// MASTER, like a CDJ's: this deck's beat goes out to the Pioneer network.
	std::function<void()> onMasterPressed;
	void setMaster (bool isMaster) { masterButton.setToggleState (isMaster, juce::dontSendNotification); }

	// The deck's own controls for the session (tempo, range, vinyl, repeat,
	// SYNC); restoring SYNC is the caller's, through onSyncToggled.
	// Grid Adjust, as on a CDJ-3000: GRID turns it on; the jog wheel then moves
	// the grid, and <1/2 1/2> <1 1> (the one a beat) SNAP (the downbeat onto the cue) SHIFT (take the
	// phase aligned by ear against the sync leader) RESET (as analysed) edit it.
	enum class GridAction { shift, halfBack, halfForward, oneBack, oneForward, snapToCue, downbeatAtPlayhead, shiftToLeader, reset };

	// The parts in the band: made children of `parent`, placed by it.
	void addBandPartsTo (juce::Component& parent);
	void setBandBounds (juce::Rectangle<int> overviewArea, juce::Rectangle<int> pitchArea,
						juce::Rectangle<int> valueArea, juce::Rectangle<int> rangeArea);
	std::function<void (GridAction, double seconds)> onGridEdit;

	void saveState (DeckSession& state) const;
	void restoreState (const DeckSession& state);

	void paint (juce::Graphics& g) override;
	void resized() override;

private:
	void setTempoRange (double range);

	StemDeckPlayer& player;
	const int deckIndex;
	bool previewingFromCue = false;
	bool cueButtonWasDown = false;
	bool analysing = false;
	bool settingTempoFromSync = false;
	double tempoRange = 0.08;

	juce::Label titleLabel, stemsLabel, elapsedLabel, remainingLabel;
	juce::Label bpmLabel, bpmInfoLabel;
	OverviewWaveform overview;
	JogWheel jog;

	juce::TextButton previousButton { juce::String::fromUTF8 ("|\xe2\x97\x80") }, nextButton { juce::String::fromUTF8 ("\xe2\x96\xb6|") };
	juce::TextButton cueButton { "CUE" }, playButton { "PLAY" }, loopOffButton { "LOOP OFF" }, repeatButton { "REPEAT" };
	juce::TextButton syncButton { "SYNC" }, masterButton { "MASTER" }, vinylButton { "VINYL" };
	juce::TextButton gridButton { "GRID" };
	juce::TextButton halfBackButton { juce::String::fromUTF8 ("\xe2\x80\xb9" "\xc2\xbd") }, halfForwardButton { juce::String::fromUTF8 ("\xc2\xbd" "\xe2\x80\xba") };
	juce::TextButton snapButton { "SNAP" }, shiftButton { "SHIFT" }, resetGridButton { "RESET" };
	juce::TextButton downbeatButton { "SET 1" };
	juce::TextButton oneBackButton { juce::String::fromUTF8 ("\xe2\x97\x80" " 1") }, oneForwardButton { juce::String::fromUTF8 ("1 " "\xe2\x96\xb6") };
	void setGridMode (bool on);
	void showPitchValue();
	juce::Slider pitchFader { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
	juce::Label pitchValue;
	juce::TextButton rangeKey;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeckPanel)
};
