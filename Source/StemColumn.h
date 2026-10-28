// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneShotKeys.h"
#include "Session.h"
#include "StemViewer.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>

class DeckSpatialPorts;
class StemDeckPlayer;

// One stem of the one-deck screen (v4, spec section 9), top to bottom: its
// name, the state line (home, armed, running with its beat count, the AUX
// send when there is one), the knobs two by two (GAIN, 3D; AUX, WIDTH), the viewer (StemViewer: home dragged, a shot shown),
// the one-shot pads two by two, CUE and MUTE, the length.
// GAIN, CUE and MUTE are the player's; 3D, AUX, WIDTH and the home are the
// spatial settings (DeckSpatialPorts::settings), never the played pose.
class StemColumn : public juce::Component
{
public:
	StemColumn (StemDeckPlayer&, DeckSpatialPorts&, int stem, std::function<bool()> takeShift);

	void setStemName (const juce::String&);
	// The owner's timer: the shot's state, the viewer, CUE and MUTE as the player has them.
	void refresh();

	// GAIN, MUTE and CUE for the session; the spatial values are DeckHost's.
	void saveState (StemMixState&) const;
	void restoreState (const StemMixState&);

	juce::Slider& gain() { return gainKnob; }
	juce::Slider& threeD() { return threeDKnob; }
	juce::Slider& aux() { return auxKnob; }
	juce::Slider& width() { return widthKnob; }
	juce::TextButton& cue() { return cueKey; }
	juce::TextButton& mute() { return muteKey; }
	StemViewer& viewer() { return stemViewer; }
	OneShotKeys& oneShots() { return keys; }
	juce::String stateText() const { return stateLabel.getText(); }
	juce::String auxCaptionText() const { return auxCaption.getText(); }

	// Where the hardware pot for AUX stands while it has not picked the knob up (R13); none hides the mark.
	void setAuxGhost (std::optional<double> position);

	void paint (juce::Graphics&) override;
	void paintOverChildren (juce::Graphics&) override;
	void resized() override;

private:
	void homeDragged (spatial::Pose);
	void showShares();   // the 3D and AUX captions carry their values

	StemDeckPlayer& player;
	DeckSpatialPorts& ports;
	const int stem;
	const juce::Colour colour;

	juce::Label nameLabel, stateLabel;
	std::optional<double> auxGhost;
	juce::Slider gainKnob, threeDKnob, auxKnob, widthKnob;
	juce::Label gainCaption, threeDCaption, auxCaption, widthCaption;
	StemViewer stemViewer;
	OneShotKeys keys;
	juce::TextButton cueKey { "CUE" }, muteKey { "MUTE" };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StemColumn)
};
