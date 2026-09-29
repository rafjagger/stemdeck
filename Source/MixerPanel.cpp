#include "MixerPanel.h"
#include "Theme.h"
#include "MeterBallistics.h"

namespace
{
	constexpr float minDb = -60.0f; // bottom of knobs and fader = silence

	juce::String busName (int bus) { return juce::String (buses::name (bus)); }

	// A lit bus switch: the stem's own colour on 1-4, AUX and PHONES their own.
	juce::Colour busColour (int bus, int stem)
	{
		return bus == buses::aux ? Theme::aux : bus == buses::phones ? Theme::cue : Theme::stem (stem);
	}

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

	g.setFont (juce::FontOptions (10.0f, juce::Font::bold));

	for (int bus = 0; bus < numBuses; ++bus)
	{
		const auto left = meters[bus * 2]->getBounds();
		const auto right = meters[bus * 2 + 1]->getBounds();
		const auto pair = left.getUnion (right);

		g.setColour (bus < buses::aux ? Theme::text : busColour (bus, 0));
		g.drawText (busName (bus), pair.withY (labelArea.getY()).withHeight (14).expanded (6, 0), juce::Justification::centred);
		g.setColour (Theme::textDim);
		g.drawText ("L", left.withY (labelArea.getY() + 14).withHeight (12), juce::Justification::centred);
		g.drawText ("R", right.withY (labelArea.getY() + 14).withHeight (12), juce::Justification::centred);
	}
}

void OutputMeters::resized()
{
	// Bars top-aligned with the faders; captions where the PHONES buttons are.
	auto area = getLocalBounds().withTrimmedLeft (4).withTrimmedRight (4);
	labelArea = area.removeFromBottom (28);

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

		for (int bus = 0; bus < buses::count; ++bus)
		{
			auto* button = busButtons.add (new juce::TextButton (juce::String (bus + 1)));   // 5 = AUX, 6 = PH
			button->setClickingTogglesState (true);
			button->setToggleState (player.isStemOnBus (s, bus), juce::dontSendNotification);
			button->setColour (juce::TextButton::buttonOnColourId, busColour (bus, s));
			button->setMouseClickGrabsKeyboardFocus (false);
			button->onClick = [this, s, bus, button] { player.setStemOnBus (s, bus, button->getToggleState()); };
			button->setTooltip (bus == buses::phones ? juce::String ("Stem auf PHONES (pre Fader)")
													 : "Stem auf Bus " + busName (bus) + " (post Fader)");
			addAndMakeVisible (button);
		}

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

	phonesButton.setClickingTogglesState (true);
	phonesButton.setColour (juce::TextButton::buttonOnColourId, busColour (buses::phones, 0));
	phonesButton.setMouseClickGrabsKeyboardFocus (false);
	phonesButton.onClick = [this] { player.setDeckPhones (phonesButton.getToggleState()); };
	phonesButton.setTooltip ("Ganzes Deck auf PHONES (pre Fader)");
	addAndMakeVisible (phonesButton);
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

	for (const auto& frame : stemFrames)
	{
		g.setColour (Theme::panel);
		g.fillRoundedRectangle (frame.toFloat(), 4.0f);
		g.setColour (Theme::outline);
		g.drawRoundedRectangle (frame.toFloat().reduced (0.5f), 4.0f, 1.0f);
	}
}

void ChannelStrip::resized()
{
	auto area = getLocalBounds().reduced (8);
	area.removeFromTop (24);

	// Four stem rows, each in a frame: [knob][name / mute][bus switches 1 2 3 / 4 5 6, right-aligned]
	const auto rowHeight = 52;
	const auto rowGap = 6;
	const auto columns = 3;
	const auto rows = buses::count / columns;
	const auto switchSize = rowHeight / rows;   // square, as big as the row allows

	for (int s = 0; s < StemSet::numStems; ++s)
	{
		if (s > 0)
			area.removeFromTop (rowGap);
		stemFrames[(size_t) s] = area.removeFromTop (rowHeight + 8);
		auto row = stemFrames[(size_t) s].reduced (4);

		knobs[s]->setBounds (row.removeFromLeft (rowHeight).withSizeKeepingCentre (rowHeight - 4, rowHeight - 4));
		row.removeFromLeft (4);

		auto grid = row.removeFromRight (switchSize * columns);
		for (int bus = 0; bus < buses::count; ++bus)
			busButtons[s * buses::count + bus]->setBounds (juce::Rectangle<int> (grid.getX() + (bus % columns) * switchSize,
																				 grid.getY() + (bus / columns) * switchSize,
																				 switchSize, switchSize).reduced (1));

		row.removeFromRight (4);
		stemLabels[s]->setBounds (row.removeFromTop (row.getHeight() / 2));
		muteButtons[s]->setBounds (row.removeFromLeft (28).reduced (0, 2));
	}

	// Below: fader, deck meter and PHONES; the output meters take the side
	// towards the middle (deck A: right, deck B: left).
	area.removeFromTop (8);
	meterZone = deckIndex == 0 ? area.removeFromRight (meterReserve) : area.removeFromLeft (meterReserve);
	phonesButton.setBounds (area.removeFromBottom (24).reduced (0, 1));
	area.removeFromBottom (4);
	// Deck meter on the outer side, mirrored: A left of its fader, B right.
	if (deckIndex == 0)
	{
		meter.setBounds (area.removeFromLeft (10).reduced (0, 4));
		area.removeFromLeft (4);
	}
	else
	{
		meter.setBounds (area.removeFromRight (10).reduced (0, 4));
		area.removeFromRight (4);
	}
	fader.setBounds (area);
}

//==============================================================================
MixerPanel::MixerPanel (StemDeckPlayer& playerA, StemDeckPlayer& playerB)
	: stripA (playerA, 0), stripB (playerB, 1)
{
	addAndMakeVisible (stripA);
	addAndMakeVisible (stripB);
	addAndMakeVisible (outputMeters);   // last: over the strips' inner corners
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
	const auto metersWidth = 170;

	// The strips meet in the middle; below the stems, each leaves half the
	// output meters' width free on its inner side, and the meters sit there,
	// as tall as the faders.
	const auto width = (area.getWidth() - gap) / 2;
	for (auto* strip : { &stripA, &stripB })
		strip->setMeterReserve ((metersWidth - gap) / 2);
	stripA.setBounds (area.removeFromLeft (width));
	stripB.setBounds (area.removeFromRight (width));
	outputMeters.setBounds (stripA.getMeterZone().translated (stripA.getX(), stripA.getY())
								.getUnion (stripB.getMeterZone().translated (stripB.getX(), stripB.getY())));
}
