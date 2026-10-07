#include "MixerPanel.h"
#include "Theme.h"

namespace
{
	constexpr float minDb = -60.0f; // bottom of knobs and fader = silence
	const auto clipColour = juce::Colour (0xffe04848);

	juce::String busName (int bus) { return juce::String (buses::name (bus)); }

	// A lit bus switch: the stem's own colour on 1-4, AUX its own.
	juce::Colour busColour (int bus, int stem)
	{
		return bus == buses::aux ? Theme::aux : Theme::stem (stem);
	}

	juce::String dbText (double db)
	{
		return db <= minDb ? juce::String ("-inf dB") : juce::String (db, 1) + " dB";
	}
}

//==============================================================================
float LevelMeter::secondsSinceLastFeed()
{
	// The meter's own clock: the main timer's 60 Hz is a wish, not a promise,
	// and the fall is 20 dB a second, not per so many ticks.
	const auto now = juce::Time::getMillisecondCounterHiRes();
	const auto seconds = lastFeedMs > 0.0 ? (now - lastFeedMs) / 1000.0 : 0.0;
	lastFeedMs = now;
	return (float) seconds;
}

void LevelMeter::setLevel (float newPeak)
{
	const auto seconds = secondsSinceLastFeed();
	ballistics.feed (newPeak, seconds);
	const auto nextClipping = showsClip && clipHold.feed (newPeak, seconds);

	const auto numSegments = meterSegments ((float) getHeight());
	const auto barSegments = showsClip ? numSegments - 1 : numSegments;
	const auto nextLit = segmentsLit (ballistics.levelDb(), barSegments);
	const auto nextHeld = segmentsLit (ballistics.holdDb(), barSegments);

	if (nextLit != litSegments || nextHeld != heldSegment || nextClipping != clipping)
	{
		litSegments = nextLit;
		heldSegment = nextHeld;
		clipping = nextClipping;
		repaint();
	}
}

void LevelMeter::paint (juce::Graphics& g)
{
	const auto bounds = getLocalBounds().toFloat();
	g.setColour (juce::Colours::black);
	g.fillRoundedRectangle (bounds, 2.0f);

	// Segmented LED bar on the desk's LED scale and in its LEDs' colours; with
	// a clip lamp, the top segment is the lamp and the bar ends at full scale
	// one below it.
	const int numSegments = meterSegments (bounds.getHeight());
	const int barSegments = showsClip ? numSegments - 1 : numSegments;
	const auto segmentHeight = bounds.getHeight() / (float) numSegments;

	const auto segmentAt = [&] (int i)
	{
		return juce::Rectangle<float> (bounds.getX() + 1.0f, bounds.getBottom() - segmentHeight * (float) (i + 1),
									   bounds.getWidth() - 2.0f, segmentHeight - 1.0f);
	};
	const auto colourOf = [barSegments] (int i)
	{
		switch (zoneOfSegment (i, barSegments))
		{
			case MeterZone::red:    return clipColour;
			case MeterZone::yellow: return juce::Colour (0xffe8c33d);
			case MeterZone::green:  return juce::Colour (0xff3ec46d);
		}
		return juce::Colour (0xff3ec46d);
	};

	for (int i = 0; i < barSegments; ++i)
	{
		g.setColour (i < litSegments ? colourOf (i) : colourOf (i).withAlpha (0.12f));
		g.fillRect (segmentAt (i));
	}

	// The held peak: a thin line, the top of its segment, above the bar.
	if (heldSegment > litSegments)
	{
		const auto held = heldSegment - 1;
		auto light = segmentAt (held);
		g.setColour (colourOf (held));
		g.fillRect (light.removeFromTop (juce::jmax (1.0f, light.getHeight() * 0.4f)));
	}

	if (! showsClip)
		return;

	g.setColour (clipping ? clipColour.brighter (0.4f) : clipColour.withAlpha (0.12f));
	g.fillRect (segmentAt (numSegments - 1));
}

//==============================================================================
OutputMeters::OutputMeters()
{
	for (int ch = 0; ch < numChannels; ++ch)
	{
		auto* meter = meters.add (new LevelMeter());
		meter->setShowsClip (true);
		addAndMakeVisible (meter);
	}
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
	// Bars top-aligned with the faders; captions below them, level with the
	// strips' empty bottom line.
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
			const auto label = bus == buses::aux ? juce::String ("A") : juce::String (bus + 1);
			auto* button = busButtons.add (new juce::TextButton (label));
			button->setClickingTogglesState (true);
			button->setToggleState (player.isStemOnBus (s, bus), juce::dontSendNotification);
			button->setColour (juce::TextButton::buttonOnColourId, busColour (bus, s));
			button->setMouseClickGrabsKeyboardFocus (false);
			button->onClick = [this, s, bus, button]
			{
				player.setStemOnBus (s, bus, button->getToggleState());
				if (onBusesChanged)
					onBusesChanged (s);
			};
			button->setTooltip ("Stem to bus " + busName (bus) + " (post fader)");
			addAndMakeVisible (button);
		}

		auto* label = stemLabels.add (new juce::Label ({}, "Stem " + juce::String (s + 1)));
		label->setFont (juce::FontOptions (10.0f, juce::Font::bold));
		label->setColour (juce::Label::textColourId, Theme::stem (s));
		label->setJustificationType (juce::Justification::centredLeft);
		label->setMinimumHorizontalScale (0.45f);   // "3 vocals" in the 400 px mixer
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
		// "1 - drums" already carries its number; "DUB" or "Vox" gets one.
		const auto& name = names[(size_t) s];
		const auto numbered = juce::CharacterFunctions::isDigit (name[0]);
		stemLabels[s]->setText (numbered ? name : juce::String (s + 1) + " " + name, juce::dontSendNotification);
		stemLabels[s]->setTooltip (names[(size_t) s]);
	}
}

void ChannelStrip::saveState (DeckSession& state) const
{
	for (int s = 0; s < StemSet::numStems; ++s)
	{
		auto& stem = state.stems[(size_t) s];
		stem.gainDb = knobs[s]->getValue();
		stem.muted = muteButtons[s]->getToggleState();
		stem.buses = 0;
		for (int bus = 0; bus < buses::count; ++bus)
			if (busButtons[s * buses::count + bus]->getToggleState())
				stem.buses |= 1u << bus;
	}
	state.faderDb = fader.getValue();
}

void ChannelStrip::restoreState (const DeckSession& state)
{
	// Through the controls, so the player follows exactly as on a click.
	for (int s = 0; s < StemSet::numStems; ++s)
	{
		const auto& stem = state.stems[(size_t) s];
		knobs[s]->setValue (stem.gainDb, juce::sendNotificationSync);
		muteButtons[s]->setToggleState (stem.muted, juce::sendNotificationSync);
		for (int bus = 0; bus < buses::count; ++bus)
			busButtons[s * buses::count + bus]->setToggleState ((stem.buses >> bus) & 1u, juce::sendNotificationSync);
	}
	fader.setValue (state.faderDb, juce::sendNotificationSync);
}

void ChannelStrip::showBuses (int stem)
{
	for (int bus = 0; bus < buses::count; ++bus)
		busButtons[stem * buses::count + bus]->setToggleState (player.isStemOnBus (stem, bus), juce::dontSendNotification);
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

	// Four stem rows, each in a frame: [knob][name / mute][bus switches 1 2 3 / 4 A, right-aligned]
	const auto rowHeight = 52;
	const auto rowGap = 6;
	const auto columns = 3;
	const auto rows = (buses::count + columns - 1) / columns;
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
		// One line high, so a narrow mixer squeezes the name instead of
		// breaking it ("Ste / m 1" at 400 px).
		const auto labelRow = row.removeFromTop (row.getHeight() / 2);
		stemLabels[s]->setBounds (labelRow.withSizeKeepingCentre (labelRow.getWidth(), juce::jmin (labelRow.getHeight(), 16)));
		muteButtons[s]->setBounds (row.removeFromLeft (28).reduced (0, 2));
	}

	// Below: fader and deck meter; the output meters take the side towards
	// the middle (deck A: right, deck B: left). The bottom line stays free,
	// where the output meters have their captions, so the fader ends where
	// the meter bars do.
	area.removeFromTop (8);
	meterZone = deckIndex == 0 ? area.removeFromRight (meterReserve) : area.removeFromLeft (meterReserve);
	area.removeFromBottom (28);
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

void MixerPanel::setMeterParameters (MeterParameters parameters)
{
	stripA.setMeterParameters (parameters);
	stripB.setMeterParameters (parameters);
	outputMeters.setMeterParameters (parameters);
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
