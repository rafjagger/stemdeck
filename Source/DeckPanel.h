#pragma once

#include <JuceHeader.h>
#include "StemDeckPlayer.h"
#include "StemThumbnails.h"
#include "Waveforms.h"
#include "JogWheel.h"
#include "Session.h"

// One deck, laid out like a CDJ: title, times and overview on top; cue/play
// on the left, the jog wheel in the middle, BPM, sync and tempo range on the
// right, the tempo fader on the outer edge.
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
	bool isSyncEnabled() const { return syncButton.getToggleState(); }
	// SYNC on but only the tempo followed: bent by hand (CDJ-3000), until SHIFT.
	void setSyncBpmOnly (bool bpmOnly) { syncButton.setButtonText (bpmOnly ? "SYNC BPM" : "SYNC"); }

	std::function<void (bool enabled)> onSyncToggled;

	// MASTER, like a CDJ's: this deck's beat goes out to the Pioneer network.
	std::function<void()> onMasterPressed;
	void setMaster (bool isMaster) { masterButton.setToggleState (isMaster, juce::dontSendNotification); }

	// The deck's own controls for the session (tempo, range, vinyl, repeat,
	// SYNC); restoring SYNC is the caller's, through onSyncToggled.
	// Grid Adjust, as on a CDJ-3000: GRID turns it on; the jog wheel then moves
	// the grid, and <1/2 1/2> SNAP (the downbeat onto the cue) SHIFT (take the
	// phase aligned by ear against the sync leader) RESET (as analysed) edit it.
	enum class GridAction { shift, halfBack, halfForward, snapToCue, shiftToLeader, reset };
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

	juce::TextButton cueButton { "CUE" }, playButton { "PLAY" }, loopOffButton { "LOOP AUS" }, repeatButton { "REPEAT" };
	juce::TextButton syncButton { "SYNC" }, masterButton { "MASTER" }, rangeButton, vinylButton { "VINYL" };
	juce::TextButton gridButton { "GRID" };
	juce::TextButton halfBackButton { juce::String::fromUTF8 ("\xe2\x80\xb9" "1/2") }, halfForwardButton { juce::String::fromUTF8 ("1/2\xe2\x80\xba") };
	juce::TextButton snapButton { "SNAP" }, shiftButton { "SHIFT" }, resetGridButton { "RESET" };
	void setGridMode (bool on);
	juce::Slider tempo { juce::Slider::LinearVertical, juce::Slider::TextBoxBelow };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeckPanel)
};
