#pragma once

#include <JuceHeader.h>

// StemDeck's settings, beside the audio device's own dialog: for now where
// the stem library lives. Chosen, never typed -- the rig's touch screen has
// no keyboard to type with (2026-09-30).
class SettingsPanel : public juce::Component
{
public:
	SettingsPanel (const juce::File& libraryFolder, std::function<void (const juce::File&)> onLibraryFolder);

	void resized() override;

private:
	void choose();
	void showFolder();

	juce::Label libraryLabel { {}, "Stem library" };
	juce::Label libraryPath;
	juce::TextButton chooseButton { juce::String::fromUTF8 ("Choose\xe2\x80\xa6") };
	juce::Label libraryNote;

	std::function<void (const juce::File&)> onLibraryFolder;
	std::unique_ptr<juce::FileChooser> chooser;
	juce::File current;
};
