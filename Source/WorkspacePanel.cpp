#include "WorkspacePanel.h"
#include "Theme.h"
#include "Workspaces.h"

void WorkspacePanel::show (const std::vector<Entry>& entries)
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
	setVisible (true);
	toFront (false);
	resized();
}

void WorkspacePanel::resized()
{
	const auto switcher = switcherGeometry (getWidth());
	const auto keyHeight = switcher.listKeyHeight, gap = switcher.listGap;
	const auto height = (int) keys.size() * (keyHeight + gap) + gap;
	column = juce::Rectangle<int> (getWidth() - switcher.margin - switcher.listWidth, switcher.listTop,
								   switcher.listWidth, height)
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
