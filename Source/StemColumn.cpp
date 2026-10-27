// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "StemColumn.h"

#include "DeckSpatialPorts.h"
#include "StemDeckPlayer.h"
#include "KnobSetup.h"
#include "StemGainKnob.h"
#include "Theme.h"

namespace
{
	// Shares of the column's height, top to bottom; the viewer takes what is left.
	constexpr float nameShare = 0.06f, stateShare = 0.045f, knobShare = 0.12f, captionShare = 0.04f;
	constexpr float keyRowShare = 0.08f, gapShare = 0.012f;
	// Text a share of the line it stands in.
	constexpr float nameFont = 0.8f, lineFont = 0.85f;

	void setUpKnob (juce::Slider& knob, juce::Range<double> range, double step, double fresh, const juce::String& suffix)
	{
		knobsetup::rotaryKnob (knob, range.getStart(), range.getEnd(), step, fresh);
		knob.setTextValueSuffix (suffix);
	}

	void setUpSwitch (juce::TextButton& key, juce::Colour on, juce::Colour inkOn)
	{
		key.setClickingTogglesState (true);
		knobsetup::untabbableKey (key);
		key.setColour (juce::TextButton::buttonOnColourId, on);
		key.setColour (juce::TextButton::textColourOnId, inkOn);
	}

	// The knob follows a value it does not own, unless a finger is on it.
	void follow (juce::Slider& knob, float value)
	{
		if (! knob.isMouseButtonDown())
			knob.setValue (value, juce::dontSendNotification);
	}
}

StemColumn::StemColumn (StemDeckPlayer& p, DeckSpatialPorts& spatialPorts, int stemIndex, std::function<bool()> takeShift)
	: player (p), ports (spatialPorts), stem (stemIndex), colour (Theme::stem (stemIndex)),
	  keys (spatialPorts.motion(), stemIndex, std::move (takeShift))
{
	const auto s = (size_t) stem;
	auto& settings = ports.settings();

	nameLabel.setColour (juce::Label::textColourId, colour);
	nameLabel.setJustificationType (juce::Justification::centred);
	nameLabel.setMinimumHorizontalScale (0.6f);
	stateLabel.setColour (juce::Label::textColourId, Theme::textDim);
	stateLabel.setJustificationType (juce::Justification::centred);
	stateLabel.setMinimumHorizontalScale (0.6f);
	setStemName ("STEM " + juce::String (stem + 1));   // until a set says
	addAndMakeVisible (nameLabel);
	addAndMakeVisible (stateLabel);

	stemgain::setUp (gainKnob);
	gainKnob.onValueChange = [this] { player.setStemGain (stem, stemgain::gainOf (gainKnob.getValue())); };
	setUpKnob (threeDKnob, { 0.0, 1.0 }, 0.01, 0.0, {});
	threeDKnob.setValue (settings.threeD[s].load(), juce::dontSendNotification);
	threeDKnob.onValueChange = [this, s]
	{
		ports.settings().threeD[s].store ((float) threeDKnob.getValue());
		showThreeD();
	};
	setUpKnob (widthKnob, { 0.0, 360.0 }, 1.0, spatial::Pose{}.width, juce::String::fromUTF8 ("\xc2\xb0"));
	widthKnob.setValue (settings.width[s].load(), juce::dontSendNotification);
	widthKnob.onValueChange = [this, s] { ports.settings().width[s].store ((float) widthKnob.getValue()); };

	for (auto [knob, caption, text] : { std::tuple { &gainKnob, &gainCaption, "GAIN" }, std::tuple { &threeDKnob, &threeDCaption, "3D" },
										std::tuple { &widthKnob, &widthCaption, "WIDTH" } })
	{
		knob->setColour (juce::Slider::rotarySliderFillColourId, colour);
		caption->setText (text, juce::dontSendNotification);
		caption->setColour (juce::Label::textColourId, Theme::textDim);
		caption->setJustificationType (juce::Justification::centred);
		caption->setMinimumHorizontalScale (0.6f);
		addAndMakeVisible (knob);
		addAndMakeVisible (caption);
	}
	showThreeD();

	stemViewer.setColour (StemViewer::stemColourId, colour);
	stemViewer.onHomeChanged = [this] (spatial::Pose pose) { homeDragged (pose); };
	addAndMakeVisible (stemViewer);

	keys.addTo (*this);
	keys.setStemColour (colour);
	keys.length().setColour (juce::Label::textColourId, Theme::text);

	setUpSwitch (cueKey, Theme::cue, juce::Colours::black);
	setUpSwitch (muteKey, Theme::mute, Theme::text);
	cueKey.setTooltip ("This stem into the cue (pre-mute)");
	muteKey.setTooltip ("This stem off the B-format and the AUX; the cue keeps it");
	cueKey.onClick = [this] { player.setStemCue (stem, cueKey.getToggleState()); };
	muteKey.onClick = [this] { player.setStemMuted (stem, muteKey.getToggleState()); };
	addAndMakeVisible (cueKey);
	addAndMakeVisible (muteKey);

	refresh();
}

void StemColumn::setStemName (const juce::String& name)
{
	nameLabel.setText (name.toUpperCase(), juce::dontSendNotification);
}

void StemColumn::showThreeD()
{
	threeDCaption.setText ("3D " + juce::String (juce::roundToInt (threeDKnob.getValue() * 100.0)), juce::dontSendNotification);
}

void StemColumn::homeDragged (spatial::Pose pose)
{
	auto& settings = ports.settings();
	const auto s = (size_t) stem;
	settings.azimuth[s].store (pose.azimuth);
	settings.elevation[s].store (pose.elevation);
	settings.width[s].store (pose.width);
	widthKnob.setValue (pose.width, juce::dontSendNotification);
}

void StemColumn::refresh()
{
	const auto& settings = ports.settings();
	const auto& motion = ports.motion();
	const auto s = (size_t) stem;
	const spatial::Pose home { settings.azimuth[s].load(), settings.elevation[s].load(), settings.width[s].load() };
	const auto state = motion.stemState (stem);

	stateLabel.setText (OneShotKeys::stateLine (motion, stem), juce::dontSendNotification);
	stateLabel.setColour (juce::Label::textColourId, state == motionlite::PadState::running ? colour
													 : state == motionlite::PadState::armed ? Theme::text : Theme::textDim);
	stemViewer.setShown (home, motion.posePlayed (stem), state);
	keys.refresh();

	follow (threeDKnob, settings.threeD[s].load());
	follow (widthKnob, settings.width[s].load());
	showThreeD();
	cueKey.setToggleState (player.isStemCued (stem), juce::dontSendNotification);
	muteKey.setToggleState (player.isStemMuted (stem), juce::dontSendNotification);
}

void StemColumn::saveState (StemMixState& state) const
{
	state.gainDb = gainKnob.getValue();
	state.muted = player.isStemMuted (stem);
	state.cue = player.isStemCued (stem);
}

void StemColumn::restoreState (const StemMixState& state)
{
	gainKnob.setValue (state.gainDb, juce::dontSendNotification);
	player.setStemGain (stem, stemgain::gainOf (gainKnob.getValue()));
	player.setStemMuted (stem, state.muted);
	player.setStemCue (stem, state.cue);
	refresh();
}

void StemColumn::paint (juce::Graphics& g)
{
	// A card in the panel's grey; the stem's colour only as the line on top.
	const auto card = getLocalBounds().toFloat();
	juce::Path outline;
	outline.addRoundedRectangle (card, Theme::corner (card));
	g.setColour (Theme::panel);
	g.fillPath (outline);
	const juce::Graphics::ScopedSaveState clip (g);
	g.reduceClipRegion (outline);
	g.setColour (colour);
	g.fillRect (card.withHeight (juce::jmax (2.0f, (float) getHeight() * gapShare / 2.0f)));
}

void StemColumn::resized()
{
	const auto height = (float) getHeight();
	const auto row = [height] (float share) { return juce::roundToInt (height * share); };
	const auto gap = juce::jmax (1, row (gapShare));
	auto area = getLocalBounds().reduced (gap * 2, gap * 2);
	area.removeFromTop (gap);   // below the colour line

	const auto placeText = [] (juce::Label& label, juce::Rectangle<int> bounds, float share, bool bold)
	{
		label.setBounds (bounds);
		label.setFont (juce::FontOptions ((float) bounds.getHeight() * share, bold ? juce::Font::bold : juce::Font::plain));
	};
	placeText (nameLabel, area.removeFromTop (row (nameShare)), nameFont, true);
	placeText (stateLabel, area.removeFromTop (row (stateShare)), lineFont, false);
	area.removeFromTop (gap);

	// Three knobs side by side, each with its caption under it.
	auto knobs = area.removeFromTop (row (knobShare));
	auto captions = area.removeFromTop (row (captionShare));
	const auto knobWidth = knobs.getWidth() / 3;
	for (auto [knob, caption] : { std::pair { &gainKnob, &gainCaption }, std::pair { &threeDKnob, &threeDCaption },
								  std::pair { &widthKnob, &widthCaption } })
	{
		knob->setBounds (knobs.removeFromLeft (knobWidth).reduced (gap / 2, 0));
		placeText (*caption, captions.removeFromLeft (knobWidth), lineFont, false);
	}
	area.removeFromTop (gap);

	// From the foot up: the length, CUE | MUTE, the pads two by two.
	const auto pair = [gap] (juce::Rectangle<int> r, juce::Component& left, juce::Component& right)
	{
		const auto half = (r.getWidth() - gap) / 2;
		left.setBounds (r.removeFromLeft (half));
		right.setBounds (r.removeFromRight (half));
	};
	auto lengthRow = area.removeFromBottom (row (keyRowShare));
	const auto arrowWidth = lengthRow.getWidth() / 4;
	keys.lengthDown().setBounds (lengthRow.removeFromLeft (arrowWidth));
	keys.lengthUp().setBounds (lengthRow.removeFromRight (arrowWidth));
	placeText (keys.length(), lengthRow, lineFont * 0.6f, false);
	area.removeFromBottom (gap);
	pair (area.removeFromBottom (row (keyRowShare)), cueKey, muteKey);
	area.removeFromBottom (gap);
	pair (area.removeFromBottom (row (keyRowShare)), keys.pad (motionlite::Shape::rise), keys.pad (motionlite::Shape::scat));
	area.removeFromBottom (gap);
	pair (area.removeFromBottom (row (keyRowShare)), keys.pad (motionlite::Shape::orbit), keys.pad (motionlite::Shape::ping));
	area.removeFromBottom (gap);

	const auto side = juce::jmin (area.getWidth(), area.getHeight());
	stemViewer.setBounds (area.withSizeKeepingCentre (side, side));
}
