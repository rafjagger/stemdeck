// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "WorkspaceSwitch.h"

// The Motion key and the workspace list, shared by the two-deck and the
// one-deck screen. A component needs JUCE's GUI initialised, no X window.
namespace
{
	struct Fixture
	{
		juce::ScopedJuceInitialiser_GUI init;
		std::vector<std::vector<std::string>> calls;
		std::string reply = R"([{"name":"1:MOTION","focused":false},{"name":"2:STEMDECK","focused":true}])";
		WorkspaceSwitch keys { [this] (const std::vector<std::string>& argv) { calls.push_back (argv); return reply; } };
		juce::Component window;

		Fixture()
		{
			window.setSize (768, 480);
			keys.attachTo (window);
			keys.place ({ 0, 0, 768, 50 }, 8);
		}
	};
}

TEST (WorkspaceSwitch, BothKeysAreThereAndFingertipSized)
{
	Fixture f;
	EXPECT_TRUE (f.keys.motionButton().isVisible());
	EXPECT_TRUE (f.keys.workspacesButton().isVisible());
	EXPECT_GE (f.keys.motionButton().getHeight(), 40);
	EXPECT_GE (f.keys.workspacesButton().getWidth(), 30);
}

TEST (WorkspaceSwitch, PlacedAtTheFarRightWhereMotionHasIt)
{
	Fixture f;
	const auto geometry = switcherGeometry (768);
	EXPECT_EQ (f.keys.workspacesButton().getRight(), 768 - geometry.margin);
	EXPECT_EQ (f.keys.motionButton().getRight(), f.keys.workspacesButton().getX() - geometry.gap);
}

TEST (WorkspaceSwitch, PlaceHandsBackTheBarLeftOfTheKeys)
{
	Fixture f;
	const auto rest = f.keys.place ({ 0, 0, 768, 50 }, 8);
	EXPECT_EQ (rest.getX(), 0);
	EXPECT_EQ (rest.getRight(), f.keys.motionButton().getX() - 8);
}

TEST (WorkspaceSwitch, MotionGoesToWorkspaceOne)
{
	Fixture f;
	f.keys.motionButton().onClick();
	ASSERT_EQ (f.calls.size(), 1u);
	EXPECT_EQ (f.calls[0], (std::vector<std::string> { "i3-msg", "-q", "workspace number 1" }));
}

TEST (WorkspaceSwitch, TheArrowOpensTheListAndAKeyGoesThere)
{
	Fixture f;
	EXPECT_FALSE (f.keys.panel().isVisible());
	f.keys.workspacesButton().onClick();
	EXPECT_TRUE (f.keys.panel().isVisible());
	EXPECT_EQ (f.keys.panel().getBounds(), f.window.getLocalBounds());

	f.keys.panel().onChosen (2);
	EXPECT_EQ (f.calls.back(), (std::vector<std::string> { "i3-msg", "-q", "workspace number 2" }));
}

TEST (WorkspaceSwitch, NoListFromI3KeepsThePanelClosed)
{
	Fixture f;
	f.reply.clear();
	f.keys.workspacesButton().onClick();
	EXPECT_FALSE (f.keys.panel().isVisible());
}
