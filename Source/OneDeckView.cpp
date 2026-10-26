// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneDeckView.h"

#include "KnobSetup.h"
#include "SurfaceJuce.h"
#include "SurfaceLayout.h"
#include "Theme.h"

namespace
{
	// The outputs row's boxes, shares of its width: B-format (16 bars), cue, AUX, phones.
	constexpr std::array<float, 4> boxShares { 0.34f, 0.19f, 0.19f, 0.28f };
	// A box's title and caption lines, shares of its height; a meter bar, a share of its box's width.
	constexpr float titleShare = 0.22f, captionShare = 0.2f, stereoBarShare = 0.09f, fontShare = 0.8f;

	// The view, top to bottom, shares of its height: the rolling waveform, the
	// overview, the outputs row; the deck panel and the stem columns take the rest.
	constexpr float waveShare = 0.15f, overviewShare = 0.06f, outputsShare = 0.15f;
	// The deck panel's card, a share of the width (the two-deck deck column's).
	constexpr float deckCardShare = 0.23f;
	// Inside the card, shares of its height: title, stems, the DeckPanel column, BPM, original.
	constexpr float titleLine = 0.05f, stemsLine = 0.04f, columnShare = 0.52f, bpmLine = 0.07f, bpmInfoLine = 0.04f;
	constexpr float pitchColumnShare = 0.32f;   // of the card's width: the fader beside value, range, KEY and SHIFT

	void setUpPhonesKnob (juce::Slider& knob, double fresh, std::function<juce::String (double)> text)
	{
		knobsetup::rotaryKnob (knob, 0.0, 1.0, 0.01, fresh);
		knob.textFromValueFunction = std::move (text);
		// Neutral: the phones are no stem and no state.
		knob.setColour (juce::Slider::rotarySliderFillColourId, Theme::textDim);
	}

	void setUpCaption (juce::Label& label, const juce::String& text, juce::Component& parent)
	{
		label.setText (text, juce::dontSendNotification);
		label.setColour (juce::Label::textColourId, Theme::textDim);
		label.setJustificationType (juce::Justification::centred);
		label.setMinimumHorizontalScale (0.6f);
		parent.addAndMakeVisible (label);
	}

	// Bars side by side, centred in `area`, each `barWidth` wide with a gap of a third of it.
	void placeBars (juce::OwnedArray<LevelMeter>& meters, juce::Rectangle<int> area, int barWidth)
	{
		const auto gap = juce::jmax (1, barWidth / 3);
		const auto total = meters.size() * barWidth + (meters.size() - 1) * gap;
		auto row = area.withSizeKeepingCentre (juce::jmin (total, area.getWidth()), area.getHeight());
		for (auto* meter : meters)
		{
			meter->setBounds (row.removeFromLeft (barWidth));
			row.removeFromLeft (gap);
		}
	}
}

//==============================================================================
OneDeckOutputs::OneDeckOutputs (OneDeckAudio& a) : audio (a)
{
	const auto addMeters = [this] (juce::OwnedArray<LevelMeter>& meters, int count, bool clipLamp)
	{
		for (int i = 0; i < count; ++i)
		{
			auto* meter = meters.add (new LevelMeter());
			meter->setShowsClip (clipLamp);
			addAndMakeVisible (meter);
		}
	};
	addMeters (bformatMeters, outputs::bformatChannels, false);
	addMeters (cueMeters, 2, true);
	addMeters (auxMeters, 2, true);

	boxes[0].title = "OUT " + juce::String::fromUTF8 ("\xc2\xb7") + " B-format";
	boxes[1].title = "CUE " + juce::String::fromUTF8 ("\xc2\xb7") + " binaural";
	boxes[2].title = "AUX " + juce::String::fromUTF8 ("\xc2\xb7") + " stereo";
	boxes[3].title = "PHONES";
	captions[0] = juce::String (outputs::bformatChannels) + " ch " + juce::String::fromUTF8 ("\xe2\x86\x92") + " Core";
	captions[2] = juce::String::fromUTF8 ("\xe2\x86\x92") + " Core aux";

	setUpPhonesKnob (levelKnob, 1.0, [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + " %"; });
	setUpPhonesKnob (mixKnob, 0.0, [] (double v)
	{
		return v <= 0.0 ? juce::String ("CUE") : v >= 1.0 ? juce::String ("MIX") : juce::String (juce::roundToInt (v * 100.0)) + " % MIX";
	});
	levelKnob.setTooltip ("Phones level");
	mixKnob.setTooltip ("Phones: the cue at the left end, the deck's mix at the right");
	levelKnob.onValueChange = [this] { phonesMoved(); };
	mixKnob.onValueChange = [this] { phonesMoved(); };
	addAndMakeVisible (levelKnob);
	addAndMakeVisible (mixKnob);
	setUpCaption (levelCaption, "LEVEL", *this);
	setUpCaption (mixCaption, juce::String::fromUTF8 ("CUE\xe2\x86\x94MIX"), *this);
}

void OneDeckOutputs::setMeterParameters (MeterParameters parameters)
{
	for (auto* meters : { &bformatMeters, &cueMeters, &auxMeters })
		for (auto* meter : *meters)
			meter->setParameters (parameters);
}

void OneDeckOutputs::setPhones (float level, float mix)
{
	levelKnob.setValue (level, juce::dontSendNotification);
	mixKnob.setValue (mix, juce::dontSendNotification);
	audio.setPhones ((float) levelKnob.getValue(), (float) mixKnob.getValue());
}

void OneDeckOutputs::phonesMoved()
{
	const auto level = (float) levelKnob.getValue(), mix = (float) mixKnob.getValue();
	audio.setPhones (level, mix);
	if (onPhonesChanged)
		onPhonesChanged (level, mix);
}

void OneDeckOutputs::setStemNames (const std::array<juce::String, StemDeckPlayer::numStems>& names)
{
	stemNames = names;
}

void OneDeckOutputs::refresh()
{
	const auto layout = outputs::oneDeck::layout();
	for (int ch = 0; ch < bformatMeters.size(); ++ch)
		bformatMeters[ch]->setLevel (audio.popPeak (layout.bformat + ch));
	for (int side = 0; side < 2; ++side)
	{
		cueMeters[side]->setLevel (audio.popPeak (layout.cue + side));
		auxMeters[side]->setLevel (audio.popPeak (layout.aux + side));
	}

	// Which stems are in the cue, by name; and whether StemDeck's own phones run (R12).
	const auto& player = audio.host().player();
	juce::StringArray cued;
	for (int s = 0; s < StemDeckPlayer::numStems; ++s)
		if (player.isStemCued (s))
			cued.add (stemNames[(size_t) s].isNotEmpty() ? stemNames[(size_t) s].toUpperCase() : juce::String (s + 1));
	std::array<juce::String, 4> now = captions;
	now[1] = cued.isEmpty() ? juce::String ("nothing cued") : cued.joinIntoString (" ") + " cued";
	now[3] = audio.phonesDecoderOn() ? juce::String ("on") : juce::String ("off");
	if (now != shown)
	{
		shown = now;
		repaint();
	}
}

void OneDeckOutputs::paint (juce::Graphics& g)
{
	for (size_t b = 0; b < boxes.size(); ++b)
	{
		const auto& box = boxes[b];
		const auto card = box.area.toFloat();
		g.setColour (Theme::panel);
		g.fillRoundedRectangle (card, Theme::corner (card));

		const auto title = box.area.withHeight (box.meters.getY() - box.area.getY());
		g.setColour (Theme::text);
		g.setFont (juce::FontOptions ((float) title.getHeight() * fontShare * 0.75f, juce::Font::bold));
		g.drawFittedText (box.title, title.reduced (title.getHeight() / 3, 0), juce::Justification::centredLeft, 1, 0.7f);

		g.setColour (Theme::textDim);
		if (b + 1 == boxes.size())   // the phones' decoder (R12), on its title line
		{
			g.drawFittedText (shown[b], title.reduced (title.getHeight() / 3, 0), juce::Justification::centredRight, 1, 0.7f);
			continue;
		}
		g.setFont (juce::FontOptions ((float) box.caption.getHeight() * fontShare));
		g.drawFittedText (shown[b], box.caption, juce::Justification::centred, 1, 0.6f);
	}
}

void OneDeckOutputs::resized()
{
	const auto gap = surface::metrics (getParentWidth()).gap;
	auto area = getLocalBounds();
	const auto width = area.getWidth() - gap * ((int) boxes.size() - 1);
	for (size_t b = 0; b < boxes.size(); ++b)
	{
		auto& box = boxes[b];
		box.area = b + 1 == boxes.size() ? area : area.removeFromLeft (juce::roundToInt ((float) width * boxShares[b]));
		area.removeFromLeft (gap);
		auto inside = box.area.reduced (gap, gap / 2);
		inside.removeFromTop (juce::roundToInt ((float) box.area.getHeight() * titleShare));
		box.caption = inside.removeFromBottom (juce::roundToInt ((float) box.area.getHeight() * captionShare));
		box.meters = inside;
	}

	const auto stereoBar = juce::jmax (2, juce::roundToInt ((float) boxes[1].area.getWidth() * stereoBarShare));
	placeBars (bformatMeters, boxes[0].meters, juce::jmax (2, boxes[0].meters.getWidth() * 3 / (outputs::bformatChannels * 4)));
	placeBars (cueMeters, boxes[1].meters, stereoBar);
	placeBars (auxMeters, boxes[2].meters, stereoBar);

	// The phones: two knobs, their names under them where the meters' caption stands.
	auto knobs = boxes[3].meters;
	auto names = boxes[3].caption;
	const auto half = knobs.getWidth() / 2;
	levelKnob.setBounds (knobs.removeFromLeft (half));
	mixKnob.setBounds (knobs);
	for (auto* caption : { &levelCaption, &mixCaption })
		caption->setFont (juce::FontOptions ((float) names.getHeight() * fontShare));
	levelCaption.setBounds (names.removeFromLeft (half));
	mixCaption.setBounds (names);
}

//==============================================================================
OneDeckView::OneDeckView (OneDeckAudio& a, juce::AudioFormatManager& formats, int index)
	: audio (a), deckIndex (index), thumbnails (formats, thumbCache), wave (a.host().player(), thumbnails, index),
	  panel (a.host().player(), thumbnails, index), outputRow (a)
{
	auto& host = audio.host();
	addAndMakeVisible (wave);
	addAndMakeVisible (panel);
	panel.addBandPartsTo (*this);
	// Across instances on the link (S4.7): the owner makes the keys pressable
	// once it knows the truth has the link's ports.
	panel.setSyncAvailable (false);
	panel.onGridEdit = [this] (DeckPanel::GridAction action, double seconds)
	{
		audio.host().editGrid (action, seconds, std::nullopt);   // SHIFT GRID has no leader here
	};
	host.onAnalysing = [this] (bool analysing) { panel.setAnalysing (analysing); };

	shiftKey.setClickingTogglesState (true);
	knobsetup::untabbableKey (shiftKey);
	shiftKey.setColour (juce::TextButton::buttonOnColourId, Theme::deck (deckIndex));
	shiftKey.setTooltip ("The next pad starts now, not on the next bar");
	addAndMakeVisible (shiftKey);

	for (int s = 0; s < StemDeckPlayer::numStems; ++s)
	{
		auto& column = columns[(size_t) s];
		column = std::make_unique<StemColumn> (host.player(), host.spatialPorts(), s, [this] { return takeShift(); });
		addAndMakeVisible (*column);
	}
	addAndMakeVisible (outputRow);
}

OneDeckView::~OneDeckView()
{
	audio.host().onAnalysing = nullptr;
}

// SHIFT holds for one press.
bool OneDeckView::takeShift()
{
	const auto now = shiftKey.getToggleState();
	shiftKey.setToggleState (false, juce::dontSendNotification);
	return now;
}

void OneDeckView::setSet (const StemSet& set)
{
	thumbnails.setSet (set);
	panel.setSet (set);
	wave.setTitle (set.name);
	std::array<juce::String, StemDeckPlayer::numStems> names;
	for (int s = 0; s < StemDeckPlayer::numStems; ++s)
	{
		const auto& name = set.stemNames[(size_t) s];
		names[(size_t) s] = name.isNotEmpty() ? name : "STEM " + juce::String (s + 1);
		columns[(size_t) s]->setStemName (names[(size_t) s]);
	}
	outputRow.setStemNames (names);
}

void OneDeckView::refresh()
{
	wave.repaint();
	panel.refresh();
	for (auto& column : columns)
		column->refresh();
	outputRow.refresh();
}

void OneDeckView::saveState (DeckSession& deck) const
{
	panel.saveState (deck);
	for (size_t s = 0; s < columns.size(); ++s)
		columns[s]->saveState (deck.stems[s]);
}

void OneDeckView::restoreState (const DeckSession& deck)
{
	panel.restoreState (deck);
	for (size_t s = 0; s < columns.size(); ++s)
		columns[s]->restoreState (deck.stems[s]);
}

void OneDeckView::paint (juce::Graphics& g)
{
	g.fillAll (Theme::background);
	const auto card = deckCard.toFloat();
	g.setColour (Theme::panel);
	g.fillRoundedRectangle (card, Theme::corner (card));
}

void OneDeckView::resized()
{
	const auto height = (float) getHeight();
	const auto share = [] (int whole, float part) { return juce::roundToInt ((float) whole * part); };
	const auto gap = surface::metrics (getWidth()).gap;
	auto area = getLocalBounds();

	wave.setBounds (area.removeFromTop (share (getHeight(), waveShare)));
	area.removeFromTop (gap / 2);
	surface::DeckTile parts;
	parts.overview = surface::fromJuce (area.removeFromTop (share (getHeight(), overviewShare)));
	area.removeFromTop (gap);
	outputRow.setBounds (area.removeFromBottom (juce::roundToInt (height * outputsShare)));
	area.removeFromBottom (gap);

	// The deck panel's card: title and stems, DeckPanel's column, BPM, then
	// the pitch fader beside its value, range, KEY and SHIFT.
	deckCard = area.removeFromLeft (share (getWidth(), deckCardShare));
	area.removeFromLeft (gap);
	auto card = deckCard.reduced (gap);
	const auto cardHeight = card.getHeight();
	parts.title = surface::fromJuce (card.removeFromTop (share (cardHeight, titleLine)));
	parts.stems = surface::fromJuce (card.removeFromTop (share (cardHeight, stemsLine)));
	panel.setBounds (card.removeFromTop (share (cardHeight, columnShare)).expanded (gap, 0));
	parts.bpm = surface::fromJuce (card.removeFromTop (share (cardHeight, bpmLine)));
	parts.bpmInfo = surface::fromJuce (card.removeFromTop (share (cardHeight, bpmInfoLine)));
	card.removeFromTop (gap);
	parts.pitch = surface::fromJuce (card.removeFromLeft (share (card.getWidth(), pitchColumnShare)));
	card.removeFromLeft (gap);
	const auto key = (card.getHeight() - 3 * gap) / 4;
	parts.pitchValue = surface::fromJuce (card.removeFromTop (key));
	card.removeFromTop (gap);
	parts.range = surface::fromJuce (card.removeFromTop (key));
	card.removeFromTop (gap);
	parts.keyLock = surface::fromJuce (card.removeFromTop (key));
	card.removeFromTop (gap);
	shiftKey.setBounds (card);
	panel.setBandBounds (parts);

	// The four stems share the rest.
	const auto columnWidth = (area.getWidth() - gap * (StemDeckPlayer::numStems - 1)) / StemDeckPlayer::numStems;
	for (auto& column : columns)
	{
		column->setBounds (area.removeFromLeft (columnWidth));
		area.removeFromLeft (gap);
	}
}
