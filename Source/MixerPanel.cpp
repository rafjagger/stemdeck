#include "MixerPanel.h"
#include "Theme.h"
#include "SurfaceJuce.h"

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
	const auto colourOf = [this, barSegments] (int i)
	{
		switch (zoneOfSegment (i, barSegments))
		{
			case MeterZone::red:    return clipColour;
			case MeterZone::yellow: return juce::Colour (0xffe8c33d);
			case MeterZone::green:  return barColour.value_or (juce::Colour (0xff3ec46d));
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

	// The bus names under their pairs, a share of the caption line.
	g.setFont (juce::FontOptions ((float) labelArea.getHeight() * 0.6f, juce::Font::bold));

	for (int bus = 0; bus < numBuses; ++bus)
	{
		const auto pair = meters[bus * 2]->getBounds().getUnion (meters[bus * 2 + 1]->getBounds());
		g.setColour (bus < buses::aux ? Theme::text : busColour (bus, 0));
		g.drawText (busName (bus), pair.withY (labelArea.getY()).withHeight (labelArea.getHeight()).expanded (6, 0),
					juce::Justification::centred);
	}
}

void OutputMeters::resized()
{
	// Bars top-aligned with the faders; captions below them, level with the
	// strips' empty bottom line.
	auto area = getLocalBounds().withTrimmedLeft (4).withTrimmedRight (4);
	labelArea = area.removeFromBottom (surface::meterCaptionHeight (getHeight()));

	const auto pairGap = juce::jmax (2, area.getWidth() / 25);
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
				if (onBusSwitch)
					onBusSwitch (s, bus, button->getToggleState());
				else
					showBuses (s);
			};
			button->setTooltip ("Stem to bus " + busName (bus) + " (post fader)");
			addAndMakeVisible (button);
		}

		stemMeters.add (new LevelMeter())->setBarColour (Theme::stem (s));
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
	// The deck's four stems metered in the fader's slot, pre-fader: the meters
	// behind, the fader's ticks and cap drawn over them (2026-10-08).
	fader.getProperties().set (DJLookAndFeel::meterInSlot, true);
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
		// As stored; the mixer brings both decks into the rule afterwards.
		player.setStemBuses (s, stem.buses);
		showBuses (s);
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
	for (int s = 0; s < StemSet::numStems; ++s)
		stemMeters[s]->setLevel (player.popStemPeak (s));
}

void ChannelStrip::paint (juce::Graphics& g)
{
	const auto bounds = getLocalBounds().toFloat();
	g.setColour (Theme::panelRaised);
	g.fillRoundedRectangle (bounds, 6.0f);

	// The deck's bar with its name inside, the text a share of the bar.
	const auto bar = headerArea.toFloat().reduced (headerArea.getHeight() * 0.3f, headerArea.getHeight() * 0.1f);
	g.setColour (Theme::deck (deckIndex));
	g.fillRoundedRectangle (bar, bar.getHeight() * 0.2f);
	g.setColour (juce::Colours::black);
	g.setFont (juce::FontOptions (bar.getHeight() * 0.75f, juce::Font::bold));
	g.drawText ("DECK " + Theme::deckName (deckIndex), bar, juce::Justification::centred);
}

void ChannelStrip::resized()
{
	const auto layout = surface::channelStrip (surface::fromJuce (getLocalBounds()));
	headerArea = surface::toJuce (layout.header);

	for (int s = 0; s < StemSet::numStems; ++s)
	{
		const auto& row = layout.stems[(size_t) s];
		knobs[s]->setBounds (surface::toJuce (row.knob));
		muteButtons[s]->setBounds (surface::toJuce (row.mute));
		for (int bus = 0; bus < buses::count; ++bus)
			busButtons[s * buses::count + bus]->setBounds (surface::toJuce (row.buses[(size_t) bus]));
	}
}

void ChannelStrip::addFaderTo (juce::Component& parent)
{
	for (auto* meter : stemMeters)
		parent.addAndMakeVisible (meter);   // first: behind the fader
	parent.addAndMakeVisible (fader);
}

void ChannelStrip::setFaderBounds (juce::Rectangle<int> area)
{
	fader.setBounds (area);

	// The meters fill the slot over the fader's travel: from where the cap's
	// centre stands at full level to where it stands at the bottom.
	const auto travel = fader.getLocalBounds().toFloat().reduced (0.0f, DJLookAndFeel::faderCapHeight (fader) / 2.0f);
	// The four stem meters side by side in the slot, a hairline apart.
	auto slot = DJLookAndFeel::faderSlot (fader, travel).getSmallestIntegerContainer() + fader.getPosition();
	const auto width = slot.getWidth() / stemMeters.size();
	for (auto* meter : stemMeters)
		meter->setBounds (slot.removeFromLeft (width).withTrimmedRight (1));
}

//==============================================================================
static_assert (StemSet::numStems == buses::stemsPerDeck);

MixerPanel::MixerPanel (StemDeckPlayer& playerA, StemDeckPlayer& playerB)
	: players { &playerA, &playerB }, stripA (playerA, 0), stripB (playerB, 1)
{
	for (int d = 0; d < buses::decks; ++d)
		strip (d).onBusSwitch = [this, d] (int stem, int bus, bool on)
		{
			for (const auto index : switchBus (d, stem, bus, on))
				if (onBusesChanged)
					onBusesChanged (index / buses::stemsPerDeck, index % buses::stemsPerDeck);
		};
	addAndMakeVisible (stripA);
	addAndMakeVisible (stripB);
}

void MixerPanel::addBandPartsTo (juce::Component& parent)
{
	stripA.addFaderTo (parent);
	stripB.addFaderTo (parent);
	parent.addAndMakeVisible (outputMeters);
}

void MixerPanel::setBandBounds (juce::Rectangle<int> faderA, juce::Rectangle<int> faderB, juce::Rectangle<int> meters)
{
	stripA.setFaderBounds (faderA);
	stripB.setFaderBounds (faderB);
	outputMeters.setBounds (meters);
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

buses::Masks MixerPanel::busMasks() const
{
	buses::Masks masks {};
	for (int d = 0; d < buses::decks; ++d)
		for (int s = 0; s < buses::stemsPerDeck; ++s)
			masks[(size_t) buses::stemIndex (d, s)] = players[(size_t) d]->getStemBuses (s);
	return masks;
}

void MixerPanel::setBusMasks (const buses::Masks& masks)
{
	for (int d = 0; d < buses::decks; ++d)
		for (int s = 0; s < buses::stemsPerDeck; ++s)
		{
			players[(size_t) d]->setStemBuses (s, masks[(size_t) buses::stemIndex (d, s)]);
			strip (d).showBuses (s);
		}
}

std::vector<int> MixerPanel::switchBus (int deck, int stem, int bus, bool on)
{
	const auto before = busMasks();
	const auto switched = buses::stemIndex (deck, stem);
	const auto after = buses::applySwitch (before, switched, bus, on, spare);
	// Every strip re-shown, also when nothing changed: a refused click (AUX
	// off) has already toggled its button and must toggle back.
	setBusMasks (after);
	if (onDjSwitch)
		onDjSwitch (deck, stem, bus, on);
	return buses::toReport (before, after, switched);
}

void MixerPanel::route (const std::vector<buses::Route>& routes)
{
	const auto before = busMasks();
	const auto after = buses::route (before, routes);
	setBusMasks (after);
	for (int index = 0; index < buses::stemCount; ++index)
		if (before[(size_t) index] != after[(size_t) index] && onBusesChanged)
			onBusesChanged (index / buses::stemsPerDeck, index % buses::stemsPerDeck);
}

void MixerPanel::normaliseBuses()
{
	setBusMasks (buses::normalise (busMasks()));
}

void MixerPanel::paint (juce::Graphics& g)
{
	g.setColour (Theme::panel);
	g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f);
}

void MixerPanel::resized()
{
	// The strips meet in the middle; their faders and the output meters are
	// in the band below (MainComponent).
	const auto strips = surface::mixerStrips (surface::fromJuce (getLocalBounds()));
	stripA.setBounds (surface::toJuce (strips[0]));
	stripB.setBounds (surface::toJuce (strips[1]));
}
