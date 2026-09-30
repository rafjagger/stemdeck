#include "WorkspacePanel.h"
#include "Theme.h"

void WorkspacePanel::show (const std::vector<Entry>& entries, juce::Rectangle<int> anchor)
{
	keys.clear();
	for (const auto& entry : entries)
	{
		auto* key = keys.add (new juce::TextButton (entry.label));
		key->setClickingTogglesState (false);
		key->setToggleState (entry.current, juce::dontSendNotification);
		key->setColour (juce::TextButton::buttonOnColourId, Theme::panelRaised.brighter (0.3f));
		key->setMouseClickGrabsKeyboardFocus (false);
		key->onClick = [this, number = entry.number]
		{
			setVisible (false);
			if (onChosen)
				onChosen (number);
		};
		addAndMakeVisible (key);
	}
	anchorArea = anchor;
	setVisible (true);
	toFront (false);
	resized();
}

void WorkspacePanel::resized()
{
	const auto keyHeight = 40, gap = 4, width = 170;
	const auto height = (int) keys.size() * (keyHeight + gap) + gap;
	column = juce::Rectangle<int> (anchorArea.getRight() - width, anchorArea.getBottom() + 4, width, height)
				 .constrainedWithin (getLocalBounds());
	auto area = column.reduced (gap);
	for (auto* key : keys)
	{
		key->setBounds (area.removeFromTop (keyHeight));
		area.removeFromTop (gap);
	}
}

void WorkspacePanel::paint (juce::Graphics& g)
{
	g.setColour (Theme::panel);
	g.fillRoundedRectangle (column.toFloat(), 6.0f);
	g.setColour (Theme::outline);
	g.drawRoundedRectangle (column.toFloat(), 6.0f, 1.0f);
}

void WorkspacePanel::mouseUp (const juce::MouseEvent& event)
{
	if (! column.contains (event.getPosition()))
		setVisible (false);
}
