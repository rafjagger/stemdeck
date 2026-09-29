#include "DeckPanel.h"
#include "Theme.h"

namespace
{
	constexpr double tempoRanges[] = { 0.08, 0.16, 0.50 };
}

DeckPanel::DeckPanel (StemDeckPlayer& p, StemThumbnails& thumbnails, int index)
	: player (p), deckIndex (index), overview (p, thumbnails), jog (p, index)
{
	titleLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
	titleLabel.setText ("Leer - Set hierher ziehen", juce::dontSendNotification);
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
	addAndMakeVisible (jog);

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
	loopOffButton.setTooltip (juce::String::fromUTF8 ("Loop setzen: in der \xc3\x9c" "bersicht ziehen"));
	loopOffButton.setColour (juce::TextButton::buttonOnColourId, Theme::loop);

	repeatButton.setClickingTogglesState (true);
	repeatButton.setColour (juce::TextButton::buttonOnColourId, Theme::play.darker (0.3f));
	repeatButton.setTooltip ("Am Ende des Tracks wieder von vorne");
	repeatButton.onClick = [this] { player.setRepeat (repeatButton.getToggleState()); };

	syncButton.setClickingTogglesState (true);
	syncButton.setColour (juce::TextButton::buttonOnColourId, Theme::deck (deckIndex));
	syncButton.setTooltip ("Tempo und Beats an das andere Deck koppeln");
	syncButton.onClick = [this] { if (onSyncToggled) onSyncToggled (syncButton.getToggleState()); };
	masterButton.setColour (juce::TextButton::buttonOnColourId, Theme::deck (deckIndex));
	masterButton.setTooltip ("Dieses Deck gibt den Takt ins Pioneer-Netz (wie MASTER am CDJ)");
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
	vinylButton.setTooltip ("Vinyl-Modus: Jogwheel-Oberseite scratcht");
	vinylButton.onClick = [this] { jog.setVinylMode (vinylButton.getToggleState()); };

	for (auto* b : { &cueButton, &playButton, &loopOffButton, &repeatButton, &syncButton, &masterButton, &rangeButton, &vinylButton, &gridButton })
	{
		b->setMouseClickGrabsKeyboardFocus (false); // keyboard shortcuts stay with the main window
		addAndMakeVisible (b);
	}

	gridButton.setClickingTogglesState (true);
	gridButton.setColour (juce::TextButton::buttonOnColourId, Theme::loop);
	gridButton.setTooltip ("Grid Adjust: Jogwheel verschiebt das Beatgrid (wie CDJ-3000)");
	gridButton.onClick = [this] { setGridMode (gridButton.getToggleState()); };
	jog.onGridShift = [this] (double seconds) { if (onGridEdit) onGridEdit (GridAction::shift, seconds); };

	const auto gridAction = [this] (juce::TextButton& b, GridAction action, const char* tip)
	{
		b.setTooltip (juce::String::fromUTF8 (tip));
		b.setMouseClickGrabsKeyboardFocus (false);
		b.onClick = [this, action] { if (onGridEdit) onGridEdit (action, 0.0); };
		addChildComponent (b);
	};
	gridAction (halfBackButton, GridAction::halfBack, "Grid einen halben Beat fr\xc3\xbc" "her");
	gridAction (halfForwardButton, GridAction::halfForward, "Grid einen halben Beat sp\xc3\xa4ter");
	gridAction (snapButton, GridAction::snapToCue, "SNAP GRID (CUE): die Eins des Takts auf den Cue-Punkt");
	gridAction (shiftButton, GridAction::shiftToLeader, "SHIFT GRID: den nach Geh\xc3\xb6r angeglichenen Beat ins Grid \xc3\xbc" "bernehmen");
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
	for (auto* b : { &halfBackButton, &halfForwardButton, &snapButton, &shiftButton, &resetGridButton })
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

void DeckPanel::resized()
{
	auto area = getLocalBounds().reduced (12);

	// Tempo fader on the outer edge, as in Mixxx.
	auto tempoArea = deckIndex == 0 ? area.removeFromLeft (64) : area.removeFromRight (64);
	tempo.setBounds (tempoArea);
	deckIndex == 0 ? area.removeFromLeft (10) : area.removeFromRight (10);

	titleLabel.setBounds (area.removeFromTop (24));
	stemsLabel.setBounds (area.removeFromTop (18));

	auto times = area.removeFromTop (26);
	elapsedLabel.setBounds (times.removeFromLeft (times.getWidth() / 2));
	remainingLabel.setBounds (times);
	area.removeFromTop (4);
	overview.setBounds (area.removeFromTop (juce::jlimit (40, 70, area.getHeight() / 4)));
	area.removeFromTop (10);

	// Transport | jog | BPM, sync, range, vinyl
	auto left = area.removeFromLeft (84);
	auto right = area.removeFromRight (84);

	const auto buttonGap = 8, smallButton = 28, smallGap = 6;
	const auto smallButtons = 2 * smallButton + smallGap;
	const auto bigButton = juce::jmin (64, (left.getHeight() - smallButtons - 2 * buttonGap) / 2);
	cueButton.setBounds (left.removeFromTop (bigButton));
	left.removeFromTop (buttonGap);
	playButton.setBounds (left.removeFromTop (bigButton));
	left.removeFromTop (buttonGap);
	loopOffButton.setBounds (left.removeFromTop (smallButton));
	left.removeFromTop (smallGap);
	repeatButton.setBounds (left.removeFromTop (smallButton));

	bpmLabel.setBounds (right.removeFromTop (30));
	bpmInfoLabel.setBounds (right.removeFromTop (16));
	right.removeFromTop (8);
	syncButton.setBounds (right.removeFromTop (40));
	right.removeFromTop (6);
	masterButton.setBounds (right.removeFromTop (28));
	right.removeFromTop (8);
	rangeButton.setBounds (right.removeFromTop (28));
	right.removeFromTop (6);
	vinylButton.setBounds (right.removeFromTop (28));
	right.removeFromTop (6);
	gridButton.setBounds (right.removeFromTop (28));

	// Grid Adjust: its buttons in a row under the jog wheel.
	if (gridButton.getToggleState())
	{
		auto row = area.removeFromBottom (28);
		area.removeFromBottom (6);
		const auto width = row.getWidth() / 5;
		for (auto* b : { &halfBackButton, &halfForwardButton, &snapButton, &shiftButton, &resetGridButton })
			b->setBounds (row.removeFromLeft (width).reduced (2, 0));
	}

	const auto jogSize = juce::jmin (area.getWidth() - 16, area.getHeight());
	jog.setBounds (area.withSizeKeepingCentre (jogSize, jogSize));
}
