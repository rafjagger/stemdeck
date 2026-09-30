#pragma once

#include <JuceHeader.h>

// The rig's i3 workspaces as a column of keys inside StemDeck's own window.
// Not a juce::PopupMenu: that is a window of its own, and on the rig -- i3,
// no compositor -- it came up as a black screen and closed again
// (2026-09-30). Laid over the whole window; a tap beside the keys closes it.
class WorkspacePanel : public juce::Component
{
public:
	struct Entry { int number; juce::String label; bool current; };

	// `anchor`: the key it opens from, in this panel's parent's coordinates.
	void show (const std::vector<Entry>& entries, juce::Rectangle<int> anchor);

	std::function<void (int number)> onChosen;

	void paint (juce::Graphics&) override;
	void resized() override;
	void mouseUp (const juce::MouseEvent&) override;

private:
	juce::OwnedArray<juce::TextButton> keys;
	juce::Rectangle<int> anchorArea, column;
};
