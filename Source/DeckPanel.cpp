#include "DeckPanel.h"
#include "CueLight.h"
#include "Theme.h"
#include "SyncLabels.h"
#include "SurfaceJuce.h"
#include "StemNames.h"

namespace
{
	constexpr double tempoRanges[] = { 0.08, 0.16, 0.50 };
}

DeckPanel::DeckPanel (StemDeckPlayer& p, StemThumbnails& thumbnails, int index)
	: player (p), deckIndex (index), overview (p, thumbnails), jog (p, index)
{
	// Fonts are set where the labels are placed: a share of their height.
	titleLabel.setText ("Empty - drag a set here", juce::dontSendNotification);
	titleLabel.setMinimumHorizontalScale (0.7f);
	stemsLabel.setColour (juce::Label::textColourId, Theme::textDim);
	remainingLabel.setColour (juce::Label::textColourId, Theme::textDim);
	remainingLabel.setJustificationType (juce::Justification::centredRight);

	bpmLabel.setJustificationType (juce::Justification::centred);
	bpmInfoLabel.setColour (juce::Label::textColourId, Theme::textDim);
	bpmInfoLabel.setJustificationType (juce::Justification::centred);

	for (auto* l : { &elapsedLabel, &remainingLabel })
		addAndMakeVisible (l);
	titleLabel.setMinimumHorizontalScale (0.6f);
	stemsLabel.setMinimumHorizontalScale (0.6f);

	// Not on the screen since 2026-09-29: at 768 px a deck column has no room
	// for it, and the controller's jog wheels do the job. Still here, hidden:
	// vinyl mode and grid adjust are wired through it.
	addChildComponent (jog);

	// Cue reacts to press and release, not to clicks: held while any finger is on it.
	cueButton.onHold = [this] { cuePressed(); };
	cueButton.onLetGo = [this] { cueReleased(); };
	cueButton.setColour (juce::TextButton::textColourOffId, Theme::cue);
	// Lit like a CDJ's CUE (CueLight.h); updateCueLight() sets the toggle state.
	cueButton.setColour (juce::TextButton::buttonOnColourId, Theme::cue);
	cueButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
	playButton.onClick = [this] { playPressed(); };
	playButton.setColour (juce::TextButton::buttonOnColourId, Theme::play);
	loopOffButton.onClick = [this] { player.clearLoop(); };

	// Lit while looping (LOOP, OUT) or while the in point waits for OUT (IN).
	loopButton.onClick = [this] { toggleLoop(); };
	loopButton.setTooltip ("Loop: leave the loop, or go back into the last one");
	loopInButton.onClick = [this] { loopIn(); };
	loopInButton.setTooltip ("Loop in: the loop starts here");
	loopOutButton.onClick = [this] { loopOut(); };
	loopOutButton.setTooltip ("Loop out: the loop ends here and plays; while looping, out of it, playing on");
	for (auto* key : { &loopButton, &loopInButton, &loopOutButton })
	{
		key->setColour (juce::TextButton::buttonOnColourId, Theme::loop);
		key->setMouseClickGrabsKeyboardFocus (false);
	}
	loopOffButton.setTooltip (juce::String ("Set a loop: drag in the overview"));
	loopOffButton.setColour (juce::TextButton::buttonOnColourId, Theme::loop);

	// One play mode, as a CDJ has: each key turns the other off.
	for (auto* key : { &repeatButton, &continueButton })
		key->setColour (juce::TextButton::buttonOnColourId, Theme::playMode);
	repeatButton.setTooltip ("Start over at the end of the track");
	repeatButton.onClick = [this] { toggleRepeat(); };
	continueButton.setTooltip ("At the end of the track, the next one in the library list plays");
	continueButton.setMouseClickGrabsKeyboardFocus (false);
	continueButton.onClick = [this] { toggleContinue(); };
	addChildComponent (continueButton);   // shown with the one-deck transport (addTransportTo)

	syncButton.setClickingTogglesState (true);
	syncButton.setColour (juce::TextButton::buttonOnColourId, Theme::deck (deckIndex));
	syncButton.setTooltip (syncTooltip);
	syncButton.onClick = [this] { if (onSyncToggled) onSyncToggled (syncButton.getToggleState()); };
	masterButton.setColour (juce::TextButton::buttonOnColourId, Theme::deck (deckIndex));
	masterButton.setTooltip (syncLabels::masterTooltip());
	masterButton.onClick = [this] { if (onMasterPressed) onMasterPressed(); };

	rangeKey.setTooltip ("Tempo-Bereich umschalten");
	rangeKey.setMouseClickGrabsKeyboardFocus (false);
	rangeKey.onClick = [this]
	{
		const auto current = std::find (std::begin (tempoRanges), std::end (tempoRanges), tempoRange);
		const auto next = (current == std::end (tempoRanges) || current + 1 == std::end (tempoRanges)) ? tempoRanges[0] : *(current + 1);
		setTempoRange (next);
	};

	// Key lock, a CDJ's MASTER TEMPO: with the tempo controls, in the deck's colour as SYNC.
	keyLockButton.setClickingTogglesState (true);
	keyLockButton.setColour (juce::TextButton::buttonOnColourId, Theme::deck (deckIndex));
	keyLockButton.setTooltip ("Key lock: the tempo changes, the key stays");
	keyLockButton.setMouseClickGrabsKeyboardFocus (false);
	keyLockButton.onClick = [this] { player.setKeyLock (keyLockButton.getToggleState()); };

	vinylButton.setClickingTogglesState (true);
	vinylButton.setToggleState (true, juce::dontSendNotification);
	vinylButton.setColour (juce::TextButton::buttonOnColourId, Theme::panelRaised.brighter (0.3f));
	vinylButton.setTooltip ("Vinyl mode: the controller's jog platter scratches");
	vinylButton.onClick = [this] { jog.setVinylMode (vinylButton.getToggleState()); };

	previousButton.onClick = [this] { if (onStep) onStep (-1); };
	nextButton.onClick = [this] { if (onStep) onStep (1); };
	setStepsAvailable (false, false, {}, {});

	for (auto* b : std::initializer_list<juce::TextButton*> { &previousButton, &nextButton, &cueButton, &playButton, &loopOffButton, &repeatButton, &syncButton, &masterButton, &vinylButton, &gridButton })
	{
		b->setMouseClickGrabsKeyboardFocus (false); // keyboard shortcuts stay with the main window
		addAndMakeVisible (b);
	}

	gridButton.setClickingTogglesState (true);
	gridButton.setColour (juce::TextButton::buttonOnColourId, Theme::loop);
	gridButton.setTooltip (syncLabels::gridTooltip());
	gridButton.onClick = [this] { setGridMode (gridButton.getToggleState()); };
	jog.onGridShift = [this] (double seconds) { if (onGridEdit) onGridEdit (GridAction::shift, seconds); };

	const auto gridAction = [this] (juce::TextButton& b, GridAction action, const char* tip)
	{
		b.setTooltip (juce::String::fromUTF8 (tip));
		b.setMouseClickGrabsKeyboardFocus (false);
		b.onClick = [this, action] { if (onGridEdit) onGridEdit (action, 0.0); };
		addChildComponent (b);
	};
	gridAction (halfBackButton, GridAction::halfBack, "Grid half a beat earlier");
	gridAction (halfForwardButton, GridAction::halfForward, "Grid half a beat later");
	gridAction (oneBackButton, GridAction::oneBack, "The bar's one a beat earlier, the beats stay");
	gridAction (oneForwardButton, GridAction::oneForward, "The bar's one a beat later, the beats stay");
	gridAction (doubleTempoButton, GridAction::doubleTempo, "Tempo doubled: the analysis found half time");
	gridAction (halveTempoButton, GridAction::halveTempo, "Tempo halved: the analysis found double time");
	gridAction (snapButton, GridAction::snapToCue, "SNAP GRID (CUE): the bar's one onto the cue point");
	gridAction (downbeatButton, GridAction::downbeatAtPlayhead, "SET 1: the bar's one where the playhead is");
	gridAction (shiftButton, GridAction::shiftToLeader, "SHIFT GRID: take the beat matched by ear into the grid");
	gridAction (resetGridButton, GridAction::reset, "Grid wie analysiert");

	pitchFader.setValue (1.0, juce::dontSendNotification);
	pitchFader.setDoubleClickReturnValue (true, 1.0);
	pitchFader.textFromValueFunction = [] (double v)
	{
		const auto percent = (v - 1.0) * 100.0;
		return (percent >= 0.0 ? "+" : "") + juce::String (percent, 2) + "%";
	};
	pitchFader.valueFromTextFunction = [] (const juce::String& t) { return 1.0 + t.getDoubleValue() / 100.0; };
	pitchFader.setColour (juce::Slider::thumbColourId, Theme::deck (deckIndex));
	pitchFader.setTooltip ("Tempo (Doppelklick: 0%)");
	pitchFader.setMouseClickGrabsKeyboardFocus (false);
	pitchValue.setJustificationType (juce::Justification::centred);
	pitchValue.setMinimumHorizontalScale (0.7f);
	pitchFader.onValueChange = [this]
	{
		player.setSpeed (pitchFader.getValue());
		showPitchValue();

		// Moving the fader by hand takes the deck out of sync.
		if (! settingTempoFromSync && syncButton.getToggleState())
		{
			syncButton.setToggleState (false, juce::dontSendNotification);

			if (onSyncToggled)
				onSyncToggled (false);
		}
	};
	setTempoRange (0.08);
}

void DeckPanel::setTempoRange (double range)
{
	tempoRange = range;
	pitchFader.setRange (1.0 - range, 1.0 + range, 0.000001);
	showPitchValue();
	rangeKey.setButtonText (juce::String::fromUTF8 ("\xc2\xb1") + juce::String (juce::roundToInt (range * 100.0)) + "%");
}

void DeckPanel::setTempoFromSync (double rate)
{
	const auto needed = std::abs (rate - 1.0);

	if (needed > tempoRange)
	{
		auto range = tempoRanges[std::size (tempoRanges) - 1];

		for (auto r : tempoRanges)
			if (r >= needed) { range = r; break; }

		setTempoRange (range);
	}

	const juce::ScopedValueSetter<bool> svs (settingTempoFromSync, true);
	pitchFader.setValue (rate, juce::sendNotificationSync);
}

void DeckPanel::setStepsAvailable (bool previous, bool next, const juce::String& previousTip, const juce::String& nextTip)
{
	previousButton.setEnabled (previous);
	nextButton.setEnabled (next);
	previousButton.setTooltip (previousTip);
	nextButton.setTooltip (nextTip);
}

void DeckPanel::setSyncAvailable (bool available)
{
	for (auto* key : { &syncButton, &masterButton })
		key->setEnabled (available);
	syncButton.setTooltip (available ? syncTooltip : juce::String (syncLabels::linkUnavailable()));
	masterButton.setTooltip (available ? juce::String (syncLabels::masterTooltip()) : juce::String (syncLabels::linkUnavailable()));
}

playmode::Mode DeckPanel::playMode() const
{
	return playmode::fromFlags (repeatButton.getToggleState(), continueButton.getToggleState());
}

void DeckPanel::setPlayMode (playmode::Mode mode)
{
	if (mode == playmode::Mode::continuous && ! hasContinue())
		mode = playmode::Mode::single;
	repeatButton.setToggleState (mode == playmode::Mode::repeat, juce::dontSendNotification);
	continueButton.setToggleState (mode == playmode::Mode::continuous, juce::dontSendNotification);
	player.setRepeat (mode == playmode::Mode::repeat);
	player.setContinue (mode == playmode::Mode::continuous);
}

void DeckPanel::toggleContinue()
{
	if (hasContinue())
		setPlayMode (playmode::toggledContinue (playMode()));
}

void DeckPanel::setSyncTooltip (const juce::String& text)
{
	syncTooltip = text;
	if (syncButton.isEnabled())
		syncButton.setTooltip (text);
}

void DeckPanel::setGridMode (bool on)
{
	gridButton.setToggleState (on, juce::dontSendNotification);
	jog.setGridMode (on);
	for (auto* b : { &halfBackButton, &halfForwardButton, &oneBackButton, &oneForwardButton, &snapButton, &downbeatButton, &shiftButton, &resetGridButton, &doubleTempoButton, &halveTempoButton })
		b->setVisible (on);
	resized();
}

void DeckPanel::saveState (DeckSession& state) const
{
	state.tempo = pitchFader.getValue();
	state.tempoRange = tempoRange;
	state.keyLock = keyLockButton.getToggleState();
	state.vinyl = vinylButton.getToggleState();
	state.repeat = playMode() == playmode::Mode::repeat;
	state.continuePlay = playMode() == playmode::Mode::continuous;
	state.sync = syncButton.getToggleState();
}

void DeckPanel::restoreState (const DeckSession& state)
{
	setTempoRange (std::find (std::begin (tempoRanges), std::end (tempoRanges), state.tempoRange) != std::end (tempoRanges)
					   ? state.tempoRange : tempoRanges[0]);
	{
		// Not a hand on the fader: SYNC is restored after it.
		const juce::ScopedValueSetter<bool> svs (settingTempoFromSync, true);
		pitchFader.setValue (state.tempo, juce::sendNotificationSync);
	}
	keyLockButton.setToggleState (state.keyLock, juce::sendNotificationSync);
	vinylButton.setToggleState (state.vinyl, juce::sendNotificationSync);
	setPlayMode (playmode::fromFlags (state.repeat, state.continuePlay && hasContinue()));
	// Lit only where SYNC can work: a one-deck instance without the link keeps it dark.
	syncButton.setToggleState (state.sync && syncButton.isEnabled(), juce::dontSendNotification);
}

void DeckPanel::setSet (const StemSet& set)
{
	titleLabel.setText (set.name, juce::dontSendNotification);
	const auto shown = set.displayNames();
	juce::StringArray names;

	for (const auto& n : shown)
		names.add (n);

	stemsLabel.setText ("Stems: " + names.joinIntoString (" / "), juce::dontSendNotification);
	std::array<juce::String, StemSet::numStems> labels;
	for (int s = 0; s < StemSet::numStems; ++s)
		labels[(size_t) s] = juce::String (stemLaneLabel (shown[(size_t) s].toStdString(), s));
	overview.setStemNames (labels);
	cuePlay.reset();
	deckLoop.reset();
}

void DeckPanel::playPressed()
{
	cuePlay.playPressed (transport);
}

void DeckPanel::cuePressed()
{
	cuePlay.cueDown (transport);
	updateCueLight();
}

void DeckPanel::cueReleased()
{
	cuePlay.cueUp (transport);
	updateCueLight();
}

void DeckPanel::loopIn()
{
	deckLoop.in (transport);
	updateLoopLights();
}

void DeckPanel::loopOut()
{
	deckLoop.out (transport);
	updateLoopLights();
}

void DeckPanel::toggleLoop()
{
	deckLoop.toggle (transport);
	updateLoopLights();
}

void DeckPanel::loopInOrOut()
{
	deckLoop.inOrOut (transport);
	updateLoopLights();
}

void DeckPanel::updateLoopLights()
{
	const auto looping = player.hasLoop();
	loopButton.setToggleState (looping, juce::dontSendNotification);
	loopInButton.setToggleState (looping || deckLoop.isInMarked(), juce::dontSendNotification);
	loopOutButton.setToggleState (looping, juce::dontSendNotification);
}

void DeckPanel::updateCueLight()
{
	const auto light = cuelight::cueLight (player.isLoaded(), player.isPlaying(), cuePlay.isCueHeld(),
										   player.getPosition(), player.getCuePoint());
	const bool lit = light == cuelight::CueLight::steady
					 || (light == cuelight::CueLight::blink
						 && cuelight::blinkPhaseOn (juce::Time::getMillisecondCounterHiRes() / 1000.0));

	cueButton.setToggleState (lit, juce::dontSendNotification);
}

void DeckPanel::refresh()
{
	const auto position = player.getPosition();
	elapsedLabel.setText (Theme::formatTime (position), juce::dontSendNotification);
	remainingLabel.setText ("-" + Theme::formatTime (player.getLength() - position), juce::dontSendNotification);

	const auto grid = player.getBeatGrid();

	if (grid.isValid())
	{
		bpmLabel.setText (juce::String (grid.bpm * player.getEffectiveRate(), 2), juce::dontSendNotification);
		bpmInfoLabel.setText ("Original " + juce::String (grid.bpm, 2), juce::dontSendNotification);
	}
	else
	{
		bpmLabel.setText ("--", juce::dontSendNotification);
		bpmInfoLabel.setText (analysing ? "analysiere..." : juce::String(), juce::dontSendNotification);
	}

	playButton.setToggleState (player.isPlaying(), juce::dontSendNotification);
	updateCueLight();
	loopOffButton.setToggleState (player.hasLoop(), juce::dontSendNotification);
	loopOffButton.setEnabled (player.hasLoop());
	updateLoopLights();
	overview.refresh();
	jog.refresh();
}

surface::Metrics DeckPanel::metrics() const
{
	return surface::metrics (getParentWidth());
}

void DeckPanel::paint (juce::Graphics& g)
{
	const auto m = metrics();
	const auto panel = getLocalBounds().toFloat().reduced ((float) m.line * 2.0f / 3.0f);
	g.setColour (Theme::panel);
	g.fillRoundedRectangle (panel, Theme::corner (panel));

	// The transport on its own tile.
	if (! transportTile.isEmpty())
	{
		const auto tile = transportTile.toFloat();
		g.setColour (Theme::panelRaised);
		g.fillRoundedRectangle (tile, Theme::corner (tile));
	}

	// The coloured stripe on the side facing the mixer: the start of the
	// deck's line, which the window carries on around its tile in the band.
	g.setColour (Theme::deck (deckIndex));
	g.fillRect (surface::toJuce (surface::deckStripe (surface::fromJuce (getLocalBounds()), deckIndex, m)));
}

void DeckPanel::resized()
{
	const auto layout = surface::deckColumn (surface::fromJuce (getLocalBounds()), gridButton.getToggleState(), deckIndex, metrics(), transportPlace);
	transportTile = surface::toJuce (layout.transportTile);
	const auto place = [] (juce::Component& c, const surface::Rect& r) { c.setBounds (surface::toJuce (r)); };

	place (elapsedLabel, layout.elapsed);
	place (remainingLabel, layout.remaining);
	// The times a share of their height: big when GRID gives them its room.
	for (auto* time : { &elapsedLabel, &remainingLabel })
		time->setFont (juce::FontOptions ((float) time->getHeight() * 0.7f));
	if (transportPlace == surface::Transport::inColumn)
		setTransportBounds ({ layout.transportTile, layout.previous, layout.cue, layout.play, layout.next, {}, {}, {} });   // no loop block in the column
	place (loopOffButton, layout.loopOff);
	place (repeatButton, layout.repeat);
	place (continueButton, layout.continuePlay);
	place (syncButton, layout.sync);
	place (masterButton, layout.master);
	place (vinylButton, layout.vinyl);
	place (gridButton, layout.grid);

	// Hidden while GRID is off; their room then goes to the times and BPM.
	const std::array<juce::Component*, 5> nudge { &halfBackButton, &oneBackButton, &downbeatButton, &oneForwardButton, &halfForwardButton };
	const std::array<juce::Component*, 5> edit { &snapButton, &shiftButton, &resetGridButton, &doubleTempoButton, &halveTempoButton };
	for (size_t i = 0; i < nudge.size(); ++i)
		place (*nudge[i], layout.gridNudge[i]);
	for (size_t i = 0; i < edit.size(); ++i)
		place (*edit[i], layout.gridEdit[i]);
}

void DeckPanel::showPitchValue()
{
	pitchValue.setText (pitchFader.getTextFromValue (pitchFader.getValue()), juce::dontSendNotification);
}

void DeckPanel::addBandPartsTo (juce::Component& parent)
{
	for (auto* part : std::initializer_list<juce::Component*> { &overview, &pitchFader, &pitchValue, &rangeKey, &keyLockButton,
																&titleLabel, &stemsLabel, &bpmLabel, &bpmInfoLabel })
		parent.addAndMakeVisible (part);
}

void DeckPanel::addTransportTo (juce::Component& parent)
{
	transportPlace = surface::Transport::elsewhere;
	for (auto* key : std::initializer_list<juce::TextButton*> { &previousButton, &cueButton, &playButton, &nextButton, &loopButton, &loopInButton, &loopOutButton })
		parent.addAndMakeVisible (key);
	loopOffButton.setVisible (false);   // LOOP in the foot row does its job
	continueButton.setVisible (true);   // beside REPEAT, in LOOP OFF's row
	resized();
}

void DeckPanel::setTransportBounds (const surface::TransportKeys& keys)
{
	previousButton.setBounds (surface::toJuce (keys.previous));
	cueButton.setBounds (surface::toJuce (keys.cue));
	playButton.setBounds (surface::toJuce (keys.play));
	nextButton.setBounds (surface::toJuce (keys.next));
	loopButton.setBounds (surface::toJuce (keys.loop));
	loopInButton.setBounds (surface::toJuce (keys.loopIn));
	loopOutButton.setBounds (surface::toJuce (keys.loopOut));
}

void DeckPanel::setBandBounds (const surface::DeckTile& parts)
{
	// Text a share of the line it stands in.
	const auto place = [] (juce::Label& label, const surface::Rect& r, float share, bool bold)
	{
		label.setBounds (surface::toJuce (r));
		label.setFont (juce::FontOptions ((float) r.h * share, bold ? juce::Font::bold : juce::Font::plain));
	};
	overview.setBounds (surface::toJuce (parts.overview));
	pitchFader.setBounds (surface::toJuce (parts.pitch));
	rangeKey.setBounds (surface::toJuce (parts.range));
	keyLockButton.setBounds (surface::toJuce (parts.keyLock));
	place (pitchValue, parts.pitchValue, 0.6f, false);
	place (titleLabel, parts.title, 0.8f, true);
	place (stemsLabel, parts.stems, 0.8f, false);
	place (bpmLabel, parts.bpm, 0.85f, true);
	place (bpmInfoLabel, parts.bpmInfo, 0.85f, false);

	// Against the volume fader beside them, so BPM and fader read as one block.
	const auto towardsFader = deckIndex == 0 ? juce::Justification::centredRight : juce::Justification::centredLeft;
	bpmLabel.setJustificationType (towardsFader);
	bpmInfoLabel.setJustificationType (towardsFader);
}
