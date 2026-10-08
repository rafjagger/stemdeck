#include "DeckPanel.h"
#include "Theme.h"
#include "SyncLabels.h"

namespace
{
	constexpr double tempoRanges[] = { 0.08, 0.16, 0.50 };
}

DeckPanel::DeckPanel (StemDeckPlayer& p, StemThumbnails& thumbnails, int index)
	: player (p), deckIndex (index), overview (p, thumbnails), jog (p, index)
{
	titleLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
	titleLabel.setText ("Empty - drag a set here", juce::dontSendNotification);
	titleLabel.setMinimumHorizontalScale (0.7f);
	stemsLabel.setColour (juce::Label::textColourId, Theme::textDim);
	elapsedLabel.setFont (juce::FontOptions (20.0f));
	remainingLabel.setFont (juce::FontOptions (20.0f));
	remainingLabel.setColour (juce::Label::textColourId, Theme::textDim);
	remainingLabel.setJustificationType (juce::Justification::centredRight);

	bpmLabel.setFont (juce::FontOptions (24.0f, juce::Font::bold));
	bpmLabel.setJustificationType (juce::Justification::centred);
	bpmInfoLabel.setFont (juce::FontOptions (11.0f));
	bpmInfoLabel.setColour (juce::Label::textColourId, Theme::textDim);
	bpmInfoLabel.setJustificationType (juce::Justification::centred);

	for (auto* l : { &titleLabel, &stemsLabel, &elapsedLabel, &remainingLabel, &bpmLabel, &bpmInfoLabel })
		addAndMakeVisible (l);

	addAndMakeVisible (overview);
	// Not on the screen since 2026-09-29: at 768 px a deck column has no room
	// for it, and the controller's jog wheels do the job. Still here, hidden:
	// vinyl mode and grid adjust are wired through it.
	addChildComponent (jog);

	// Cue reacts to press and release, not to clicks.
	cueButton.onStateChange = [this]
	{
		const bool down = cueButton.isDown();

		if (down != cueButtonWasDown)
		{
			cueButtonWasDown = down;
			down ? cuePressed() : cueReleased();
		}
	};
	cueButton.setColour (juce::TextButton::textColourOffId, Theme::cue);
	playButton.onClick = [this] { togglePlay(); };
	playButton.setColour (juce::TextButton::buttonOnColourId, Theme::play);
	loopOffButton.onClick = [this] { player.clearLoop(); };
	loopOffButton.setTooltip (juce::String ("Set a loop: drag in the overview"));
	loopOffButton.setColour (juce::TextButton::buttonOnColourId, Theme::loop);

	repeatButton.setClickingTogglesState (true);
	repeatButton.setColour (juce::TextButton::buttonOnColourId, Theme::play.darker (0.3f));
	repeatButton.setTooltip ("Start over at the end of the track");
	repeatButton.onClick = [this] { player.setRepeat (repeatButton.getToggleState()); };

	syncButton.setClickingTogglesState (true);
	syncButton.setColour (juce::TextButton::buttonOnColourId, Theme::deck (deckIndex));
	syncButton.setTooltip ("Lock tempo and beats to the other deck");
	syncButton.onClick = [this] { if (onSyncToggled) onSyncToggled (syncButton.getToggleState()); };
	masterButton.setColour (juce::TextButton::buttonOnColourId, Theme::deck (deckIndex));
	masterButton.setTooltip (syncLabels::masterTooltip());
	masterButton.onClick = [this] { if (onMasterPressed) onMasterPressed(); };

	rangeButton.setTooltip ("Tempo-Bereich umschalten");
	rangeButton.onClick = [this]
	{
		const auto current = std::find (std::begin (tempoRanges), std::end (tempoRanges), tempoRange);
		const auto next = (current == std::end (tempoRanges) || current + 1 == std::end (tempoRanges)) ? tempoRanges[0] : *(current + 1);
		setTempoRange (next);
	};

	vinylButton.setClickingTogglesState (true);
	vinylButton.setToggleState (true, juce::dontSendNotification);
	vinylButton.setColour (juce::TextButton::buttonOnColourId, Theme::panelRaised.brighter (0.3f));
	vinylButton.setTooltip ("Vinyl mode: the controller's jog platter scratches");
	vinylButton.onClick = [this] { jog.setVinylMode (vinylButton.getToggleState()); };

	for (auto* b : { &cueButton, &playButton, &loopOffButton, &repeatButton, &syncButton, &masterButton, &rangeButton, &vinylButton, &gridButton })
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
	gridAction (snapButton, GridAction::snapToCue, "SNAP GRID (CUE): the bar's one onto the cue point");
	gridAction (downbeatButton, GridAction::downbeatAtPlayhead, "SET 1: the bar's one where the playhead is");
	gridAction (shiftButton, GridAction::shiftToLeader, "SHIFT GRID: take the beat matched by ear into the grid");
	gridAction (resetGridButton, GridAction::reset, "Grid wie analysiert");

	tempo.setValue (1.0, juce::dontSendNotification);
	tempo.setDoubleClickReturnValue (true, 1.0);
	tempo.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 20);
	tempo.textFromValueFunction = [] (double v)
	{
		const auto percent = (v - 1.0) * 100.0;
		return (percent >= 0.0 ? "+" : "") + juce::String (percent, 2) + "%";
	};
	tempo.valueFromTextFunction = [] (const juce::String& t) { return 1.0 + t.getDoubleValue() / 100.0; };
	tempo.setColour (juce::Slider::thumbColourId, Theme::deck (deckIndex));
	tempo.setTooltip ("Tempo (Doppelklick: 0%)");
	tempo.setMouseClickGrabsKeyboardFocus (false);
	tempo.onValueChange = [this]
	{
		player.setSpeed (tempo.getValue());

		// Moving the fader by hand takes the deck out of sync.
		if (! settingTempoFromSync && syncButton.getToggleState())
		{
			syncButton.setToggleState (false, juce::dontSendNotification);

			if (onSyncToggled)
				onSyncToggled (false);
		}
	};
	addAndMakeVisible (tempo);
	setTempoRange (0.08);
}

void DeckPanel::setTempoRange (double range)
{
	tempoRange = range;
	tempo.setRange (1.0 - range, 1.0 + range, 0.000001);
	tempo.updateText();
	rangeButton.setButtonText (juce::String::fromUTF8 ("\xc2\xb1") + juce::String (juce::roundToInt (range * 100.0)) + "%");
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
	tempo.setValue (rate, juce::sendNotificationSync);
}

void DeckPanel::setGridMode (bool on)
{
	gridButton.setToggleState (on, juce::dontSendNotification);
	jog.setGridMode (on);
	for (auto* b : { &halfBackButton, &halfForwardButton, &snapButton, &downbeatButton, &shiftButton, &resetGridButton })
		b->setVisible (on);
	resized();
}

void DeckPanel::saveState (DeckSession& state) const
{
	state.tempo = tempo.getValue();
	state.tempoRange = tempoRange;
	state.vinyl = vinylButton.getToggleState();
	state.repeat = repeatButton.getToggleState();
	state.sync = syncButton.getToggleState();
}

void DeckPanel::restoreState (const DeckSession& state)
{
	setTempoRange (std::find (std::begin (tempoRanges), std::end (tempoRanges), state.tempoRange) != std::end (tempoRanges)
					   ? state.tempoRange : tempoRanges[0]);
	{
		// Not a hand on the fader: SYNC is restored after it.
		const juce::ScopedValueSetter<bool> svs (settingTempoFromSync, true);
		tempo.setValue (state.tempo, juce::sendNotificationSync);
	}
	vinylButton.setToggleState (state.vinyl, juce::sendNotificationSync);
	repeatButton.setToggleState (state.repeat, juce::sendNotificationSync);
	syncButton.setToggleState (state.sync, juce::dontSendNotification);
}

void DeckPanel::setSet (const StemSet& set)
{
	titleLabel.setText (set.name, juce::dontSendNotification);
	juce::StringArray names;

	for (const auto& n : set.stemNames)
		names.add (n);

	stemsLabel.setText ("Stems: " + names.joinIntoString (" / "), juce::dontSendNotification);
	previewingFromCue = false;
}

void DeckPanel::togglePlay()
{
	previewingFromCue = false;

	if (player.isPlaying())
		player.pause();
	else
		player.play();
}

void DeckPanel::cuePressed()
{
	if (! player.isLoaded())
		return;

	if (player.isPlaying())
	{
		player.pause();
		player.setPosition (player.getCuePoint());
	}
	else if (std::abs (player.getPosition() - player.getCuePoint()) > 0.01)
	{
		player.setCuePoint (player.getPosition());
	}
	else
	{
		previewingFromCue = true;
		player.play();
	}
}

void DeckPanel::cueReleased()
{
	if (previewingFromCue)
	{
		previewingFromCue = false;
		player.pause();
		player.setPosition (player.getCuePoint());
	}
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
	loopOffButton.setToggleState (player.hasLoop(), juce::dontSendNotification);
	loopOffButton.setEnabled (player.hasLoop());
	overview.refresh();
	jog.refresh();
}

void DeckPanel::paint (juce::Graphics& g)
{
	auto bounds = getLocalBounds().toFloat().reduced (2.0f);
	g.setColour (Theme::panel);
	g.fillRoundedRectangle (bounds, 6.0f);

	// Coloured stripe on the side facing the mixer
	g.setColour (Theme::deck (deckIndex));
	g.fillRect (deckIndex == 0 ? bounds.removeFromRight (3.0f) : bounds.removeFromLeft (3.0f));
}

void DeckPanel::setTempoSpan (int top, int bottom)
{
	if (top == tempoTop && bottom == tempoBottom)
		return;
	tempoTop = top;
	tempoBottom = bottom;
	resized();
}

void DeckPanel::resized()
{
	auto area = getLocalBounds().reduced (6);
	const auto gap = 6;

	// At the foot, level with the mixer's volume faders and as tall: the pitch
	// fader on the outer edge and the small keys beside it. Everything above
	// has the deck's whole width -- the overview and the BPM got the room the
	// full-height pitch fader took (2026-09-30).
	const auto fallbackTop = area.getBottom() - 4 * (28 + gap);
	const auto bandTop = tempoBottom > tempoTop ? juce::jlimit (area.getY() + 120, area.getBottom() - 4 * (22 + gap), tempoTop)
												: fallbackTop;
	auto band = area.withTop (bandTop).withBottom (tempoBottom > tempoTop ? juce::jmin (area.getBottom(), tempoBottom) : area.getBottom());
	area.setBottom (bandTop - gap);

	tempo.setBounds (deckIndex == 0 ? band.removeFromLeft (54) : band.removeFromRight (54));   // "+0.00%" below it
	deckIndex == 0 ? band.removeFromLeft (gap) : band.removeFromRight (gap);

	const auto rowHeight = juce::jmax (22, (band.getHeight() - 3 * gap) / 4);
	const auto pairRow = [&band, gap, rowHeight] (juce::Component& a, juce::Component* b)
	{
		auto row = band.removeFromTop (rowHeight);
		band.removeFromTop (gap);
		if (b == nullptr)
		{
			a.setBounds (row);
			return;
		}
		a.setBounds (row.removeFromLeft ((row.getWidth() - gap) / 2));
		row.removeFromLeft (gap);
		b->setBounds (row);
	};
	pairRow (loopOffButton, &repeatButton);
	pairRow (syncButton, &masterButton);
	pairRow (rangeButton, &vinylButton);
	pairRow (gridButton, nullptr);

	// Above, the whole width: title, times, overview, BPM, the transport.
	titleLabel.setBounds (area.removeFromTop (22));
	stemsLabel.setBounds (area.removeFromTop (16));
	auto times = area.removeFromTop (22);
	elapsedLabel.setBounds (times.removeFromLeft (times.getWidth() / 2));
	remainingLabel.setBounds (times);
	area.removeFromTop (4);

	auto transport = area.removeFromBottom (44);
	cueButton.setBounds (transport.removeFromLeft ((transport.getWidth() - gap) / 2));
	transport.removeFromLeft (gap);
	playButton.setBounds (transport);
	area.removeFromBottom (gap);

	// Grid Adjust while GRID is on: moving the grid is the controller's jog
	// wheel; these nudge it, snap it and set its one.
	if (gridButton.getToggleState())
	{
		const auto gridRow = [&area, gap] (std::initializer_list<juce::Component*> keys)
		{
			auto row = area.removeFromBottom (26);
			area.removeFromBottom (4);
			const auto width = row.getWidth() / (int) keys.size();
			for (auto* key : keys)
				key->setBounds (row.removeFromLeft (width).reduced (1, 0));
		};
		gridRow ({ &snapButton, &shiftButton, &resetGridButton });
		gridRow ({ &halfBackButton, &downbeatButton, &halfForwardButton });
		area.removeFromBottom (gap - 4);
	}

	auto bpm = area.removeFromBottom (40);
	bpmLabel.setBounds (bpm.removeFromTop (26));
	bpmInfoLabel.setBounds (bpm);
	area.removeFromBottom (gap);
	overview.setBounds (area);
}
