#pragma once

#include <JuceHeader.h>

#include "Outputs.h"

// StemDeck's settings, beside the audio device's own dialog: where the stem
// library lives and what the outputs carry. Chosen, never typed -- the rig's touch screen has
// no keyboard to type with (2026-09-30).
class SettingsPanel : public juce::Component
{
public:
	SettingsPanel (const juce::File& libraryFolder, std::function<void (const juce::File&)> onLibraryFolder,
				   outputs::Mode outputMode, std::function<void (outputs::Mode)> onOutputMode);

	void resized() override;

private:
	void choose();
	void showFolder();

	juce::Label libraryLabel { {}, "Stem library" };
	juce::Label libraryPath;
	juce::TextButton chooseButton { juce::String::fromUTF8 ("Choose\xe2\x80\xa6") };
	juce::Label libraryNote;

	juce::Label outputLabel { {}, "Output" };
	juce::ComboBox outputChoice;
	juce::Label outputNote;

	std::function<void (const juce::File&)> onLibraryFolder;
	std::unique_ptr<juce::FileChooser> chooser;
	juce::File current;
};
