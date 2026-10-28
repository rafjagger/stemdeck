// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include "DeckPanel.h"
#include "LevelMeter.h"
#include "OneDeckAudio.h"
#include "StemColumn.h"
#include "StemThumbnails.h"
#include "Waveforms.h"

#include <array>
#include <functional>
#include <memory>

// The one-deck screen's outputs row (v4): the B-format meter (16 channels to
// Core), the cue's (B-format, decoded in Core), the AUX's, and the phones' LEVEL and CUE<->MIX.
// A bar for the cue bus and one for the AUX bus (each its W); meters only for what StemDeck sends; the phones' knobs write the deck.
class OneDeckOutputs : public juce::Component
{
public:
	explicit OneDeckOutputs (OneDeckAudio&);

	void refresh();   // the owner's timer: the meters and the captions
	void setMeterParameters (MeterParameters);
	void setPhones (float level, float mix);   // the knobs, written through to the deck
	void setStemNames (const std::array<juce::String, StemDeckPlayer::numStems>&);   // for the cue's caption
	std::function<void (float level, float mix)> onPhonesChanged;

	const LevelMeter& cueMeter() const { return cueBar; }
	const LevelMeter& auxMeter() const { return auxBar; }
	juce::Slider& phonesLevel() { return levelKnob; }
	juce::Slider& phonesMix() { return mixKnob; }

	void paint (juce::Graphics&) override;
	void resized() override;

private:
	struct Box
	{
		juce::String title;
		juce::Rectangle<int> area, meters, caption;
	};

	void phonesMoved();

	OneDeckAudio& audio;
	juce::OwnedArray<LevelMeter> bformatMeters;
	LevelMeter cueBar, auxBar;   // one bar each: the cue bus's W, the aux bus's W
	juce::Slider levelKnob, mixKnob;
	juce::Label levelCaption, mixCaption;
	std::array<Box, 4> boxes;   // B-format, cue, AUX, phones
	std::array<juce::String, 4> captions, shown;   // the fixed ones; what is painted
	std::array<juce::String, StemDeckPlayer::numStems> stemNames;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OneDeckOutputs)
};

//==============================================================================
// The deck part of the one-deck screen (v4 mockup, spec section 9): the
// rolling waveform with its four lanes and the overview; on the left the deck
// panel -- the two-deck DeckPanel (times, keys, CUE | PLAY) with its band
// parts (title, stems, BPM, pitch fader, range, KEY) placed here -- and SHIFT,
// which makes the next pad press start now; the four stem columns; the
// outputs row. The top bar and the library are the window's.
class OneDeckView : public juce::Component
{
public:
	OneDeckView (OneDeckAudio&, juce::AudioFormatManager&, int deckIndex);
	~OneDeckView() override;

	// After the deck's host has loaded it.
	void setSet (const StemSet&);
	// The owner's timer.
	void refresh();

	// The deck panel's (tempo, range, key lock, vinyl, repeat) and each
	// stem's GAIN, MUTE and CUE; the rest is DeckHost's.
	void saveState (DeckSession&) const;
	void restoreState (const DeckSession&);

	void setMeterParameters (MeterParameters parameters) { outputRow.setMeterParameters (parameters); }

	DeckPanel& deckPanel() { return panel; }
	StemColumn& column (int stem) { return *columns[(size_t) stem]; }
	OneDeckOutputs& outputs() { return outputRow; }
	ScrollingWaveform& waveform() { return wave; }
	juce::TextButton& shift() { return shiftKey; }

	void paint (juce::Graphics&) override;
	void resized() override;

private:
	bool takeShift();

	OneDeckAudio& audio;
	const int deckIndex;
	juce::AudioThumbnailCache thumbCache { 4 };
	StemThumbnails thumbnails;
	ScrollingWaveform wave;
	DeckPanel panel;
	juce::TextButton shiftKey { "SHIFT" };
	std::array<std::unique_ptr<StemColumn>, StemDeckPlayer::numStems> columns;
	OneDeckOutputs outputRow;
	juce::Rectangle<int> deckCard;   // the deck panel's card, under DeckPanel and its band parts

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OneDeckView)
};
