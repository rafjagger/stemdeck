// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "WorkspaceSwitch.h"

WorkspaceSwitch::WorkspaceSwitch (I3Runner runner)
	: i3 (std::move (runner))
{
	motion.setTooltip ("Over to A3 Motion (i3 workspace 1)");
	motion.onClick = [this] { goToWorkspace (i3, 1); };
	arrow.setTooltip ("Any of the rig's workspaces");
	arrow.onClick = [this] { showList(); };
	for (auto* key : { &motion, &arrow })
		key->setMouseClickGrabsKeyboardFocus (false);
	list.onChosen = [this] (int number) { goToWorkspace (i3, number); };
}

void WorkspaceSwitch::attachTo (juce::Component& parent)
{
	window = &parent;
	parent.addAndMakeVisible (motion);
	parent.addAndMakeVisible (arrow);
	parent.addChildComponent (list);
}

juce::Rectangle<int> WorkspaceSwitch::place (juce::Rectangle<int> topBar, int gap)
{
	// The very right of the window, where A3 Motion has it: the key under the
	// finger stays put when the workspace changes.
	const auto windowWidth = window != nullptr ? window->getWidth() : topBar.getRight();
	const auto switcher = switcherGeometry (windowWidth);
	auto area = topBar.withRight (windowWidth - switcher.margin);
	arrow.setBounds (area.removeFromRight (switcher.arrowWidth));
	area.removeFromRight (switcher.gap);
	motion.setBounds (area.removeFromRight (switcher.appKeyWidth));
	return topBar.withRight (motion.getX() - gap);
}

void WorkspaceSwitch::showList()
{
	const auto entries = listWorkspaces (i3);
	if (entries.empty() || window == nullptr)
		return;
	list.setBounds (window->getLocalBounds());
	list.show (entries);
}
