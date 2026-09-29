#include "MixerPanel.h"
#include "Theme.h"
#include "MeterBallistics.h"

namespace
{
	constexpr float minDb = -60.0f; // bottom of knobs and fader = silence

	juce::String dbText (double db)
	{
		return db <= minDb ? juce::String ("-inf dB") : juce::String (db, 1) + " dB";
	}
}

//==============================================================================
void LevelMeter::setLevel (float newPeak)
{
	const auto next = nextMeterLevel (level, newPeak);

	if (next != level)
	{
		level = next;
		repaint();
	}
}

void LevelMeter::paint (juce::Graphics& g)
{
	const auto bounds = getLocalBounds().toFloat();
	g.setColour (juce::Colours::black);
	g.fillRoundedRectangle (bounds, 2.0f);

	// Segmented LED bar, green -> yellow -> red
	const int numSegments = 24;
	const auto segmentHeight = bounds.getHeight() / (float) numSegments;
	const auto db = juce::Decibels::gainToDecibels (level, minDb);
	const auto lit = juce::roundToInt ((db - minDb) / -minDb * (float) numSegments);

	for (int i = 0; i < numSegments; ++i)
	{
		const auto colour = i >= numSegments - 2 ? juce::Colour (0xffe04848)
						  : i >= numSegments - 6 ? juce::Colour (0xffe8c33d)
												 : juce::Colour (0xff3ec46d);
		const auto segment = juce::Rectangle<float> (bounds.getX() + 1.0f, bounds.getBottom() - segmentHeight * (float) (i + 1),
													 bounds.getWidth() - 2.0f, segmentHeight - 1.0f);
		g.setColour (i < lit ? colour : colour.withAlpha (0.12f));
		g.fillRect (segment);
	}
}

//==============================================================================
OutputMeters::OutputMeters()
{
	for (int ch = 0; ch < numChannels; ++ch)
		addAndMakeVisible (meters.add (new LevelMeter()));
}

void OutputMeters::paint (juce::Graphics& g)
{
	g.setColour (Theme::panelRaised);
	g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

	g.setColour (Theme::textDim);
	g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
	g.drawText ("MAIN", getLocalBounds().removeFromTop (24), juce::Justification::centred);

	g.setFont (juce::FontOptions (10.0f, juce::Font::bold));

	for (int bus = 0; bus < numBuses; ++bus)
	{
		const auto left = meters[bus * 2]->getBounds();
		const auto right = meters[bus * 2 + 1]->getBounds();
		const auto pair = left.getUnion (right);

		g.setColour (bus == numBuses - 1 ? Theme::aux : Theme::text);
		g.drawText (busName (bus), pair.withY (labelArea.getY()).withHeight (14).expanded (6, 0), juce::Justification::centred);
		g.setColour (Theme::textDim);
		g.drawText ("L", left.withY (labelArea.getY() + 14).withHeight (12), juce::Justification::centred);
		g.drawText ("R", right.withY (labelArea.getY() + 14).withHeight (12), juce::Justification::centred);
	}
}

void OutputMeters::resized()
{
	auto area = getLocalBounds().reduced (6);
	area.removeFromTop (22);
	labelArea = area.removeFromBottom (26);

	const auto pairGap = 5;
	const auto meterWidth = (area.getWidth() - pairGap * (numBuses - 1)) / numChannels;
	auto x = area.getX() + (area.getWidth() - (meterWidth * numChannels + pairGap * (numBuses - 1))) / 2;

	for (int ch = 0; ch < numChannels; ++ch)
	{
		meters[ch]->setBounds (x, area.getY(), meterWidth - 1, area.getHeight());
		x += meterWidth + ((ch % 2 == 1) ? pairGap : 0);
	}
}

//==============================================================================
ChannelStrip::ChannelStrip (StemDeckPlayer& p, int index) : player (p), deckIndex (index)
{
	for (int s = 0; s < StemSet::numStems; ++s)
	{
		auto* knob = knobs.add (new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox));
		knob->setRange (minDb, 6.0, 0.1);
		knob->setSkewFactorFromMidPoint (-12.0);
		knob->setValue (0.0, juce::dontSendNotification);
		knob->setDoubleClickReturnValue (true, 0.0);
		knob->setPopupDisplayEnabled (true, true, nullptr);
		knob->textFromValueFunction = dbText;
		knob->setColour (juce::Slider::rotarySliderFillColourId, Theme::stem (s));
		knob->setMouseClickGrabsKeyboardFocus (false);
		knob->onValueChange = [this, s, knob]
		{
			player.setStemGain (s, juce::Decibels::decibelsToGain ((float) knob->getValue(), minDb));
		};
		addAndMakeVisible (knob);

		auto* mute = muteButtons.add (new juce::TextButton ("M"));
		mute->setClickingTogglesState (true);
		mute->setColour (juce::TextButton::buttonOnColourId, Theme::mute);
		mute->setMouseClickGrabsKeyboardFocus (false);
		mute->onClick = [this, s, mute] { player.setStemMuted (s, mute->getToggleState()); };
		mute->setTooltip ("Stem stumm");
		addAndMakeVisible (mute);

		auto* aux = auxButtons.add (new juce::TextButton ("AUX"));
		aux->setClickingTogglesState (true);
		aux->setColour (juce::TextButton::buttonOnColourId, Theme::aux);
		aux->setMouseClickGrabsKeyboardFocus (false);
		aux->onClick = [this, s, aux] { player.setStemToAux (s, aux->getToggleState()); };
		aux->setTooltip ("Stem vom Main-Bus nehmen und auf AUX schicken (post Fader)");
		addAndMakeVisible (aux);

		auto* label = stemLabels.add (new juce::Label ({}, "Stem " + juce::String (s + 1)));
		label->setFont (juce::FontOptions (11.0f, juce::Font::bold));
		label->setColour (juce::Label::textColourId, Theme::stem (s));
		label->setJustificationType (juce::Justification::centredLeft);
		label->setMinimumHorizontalScale (0.6f);
		addAndMakeVisible (label);
	}

	fader.setRange (minDb, 0.0, 0.1);
	fader.setSkewFactorFromMidPoint (-15.0);
	fader.setValue (0.0, juce::dontSendNotification);
	fader.setDoubleClickReturnValue (true, 0.0);
	fader.setPopupDisplayEnabled (true, true, nullptr);
	fader.textFromValueFunction = dbText;
	fader.setColour (juce::Slider::thumbColourId, Theme::deck (deckIndex));
	fader.setMouseClickGrabsKeyboardFocus (false);
	fader.onValueChange = [this]
	{
		player.setDeckGain (juce::Decibels::decibelsToGain ((float) fader.getValue(), minDb));
	};
	addAndMakeVisible (fader);
	addAndMakeVisible (meter);
}

void ChannelStrip::setStemNames (const std::array<juce::String, StemSet::numStems>& names)
{
	for (int s = 0; s < StemSet::numStems; ++s)
	{
		stemLabels[s]->setText (juce::String (s + 1) + " " + names[(size_t) s], juce::dontSendNotification);
		stemLabels[s]->setTooltip (names[(size_t) s]);
	}
}

void ChannelStrip::toggleMute (int stem)
{
	if (auto* b = muteButtons[stem])
		b->setToggleState (! b->getToggleState(), juce::sendNotificationSync);
}

void ChannelStrip::refresh()
{
	float peak = 0.0f;

	for (int s = 0; s < StemSet::numStems; ++s)
		peak = juce::jmax (peak, player.popStemPeak (s));

	meter.setLevel (peak);
}

void ChannelStrip::paint (juce::Graphics& g)
{
	const auto bounds = getLocalBounds().toFloat();
	g.setColour (Theme::panelRaised);
	g.fillRoundedRectangle (bounds, 6.0f);

	auto header = bounds.withHeight (24.0f);
	g.setColour (Theme::deck (deckIndex));
	g.fillRoundedRectangle (header.reduced (6.0f, 4.0f), 3.0f);
	g.setColour (juce::Colours::black);
	g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
	g.drawText ("DECK " + Theme::deckName (deckIndex), header, juce::Justification::centred);
}

void ChannelStrip::resized()
{
	auto area = getLocalBounds().reduced (8);
	area.removeFromTop (24);

	// Four knob rows: [knob][name / mute]
	const auto rowHeight = 50;

	for (int s = 0; s < StemSet::numStems; ++s)
	{
		auto row = area.removeFromTop (rowHeight);
		knobs[s]->setBounds (row.removeFromLeft (rowHeight));
		row.removeFromLeft (4);
		stemLabels[s]->setBounds (row.removeFromTop (row.getHeight() / 2));
		muteButtons[s]->setBounds (row.removeFromLeft (28).reduced (0, 2));
		row.removeFromLeft (4);
		auxButtons[s]->setBounds (row.removeFromLeft (38).reduced (0, 2));
	}

	area.removeFromTop (8);
	meter.setBounds (area.removeFromRight (10).reduced (0, 4));
	area.removeFromRight (4);
	fader.setBounds (area);
}

//==============================================================================
MixerPanel::MixerPanel (StemDeckPlayer& playerA, StemDeckPlayer& playerB)
	: stripA (playerA, 0), stripB (playerB, 1)
{
	addAndMakeVisible (stripA);
	addAndMakeVisible (outputMeters);
	addAndMakeVisible (stripB);
}

void MixerPanel::refresh()
{
	stripA.refresh();
	stripB.refresh();
}

void MixerPanel::paint (juce::Graphics& g)
{
	g.setColour (Theme::panel);
	g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f);
}

void MixerPanel::resized()
{
	auto area = getLocalBounds().reduced (8);
	const auto gap = 8;
	const auto metersWidth = 120;
	const auto width = (area.getWidth() - metersWidth - 2 * gap) / 2;
	stripA.setBounds (area.removeFromLeft (width));
	stripB.setBounds (area.removeFromRight (width));
	outputMeters.setBounds (area.reduced (gap, 0));
}
