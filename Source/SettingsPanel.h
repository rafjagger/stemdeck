#pragma once

#include <JuceHeader.h>

#include "FolderChoice.h"

// StemDeck's settings, beside the audio device's own dialog: where the stem
// library lives. Chosen, never typed -- the rig's touch screen has
// no keyboard to type with (2026-09-30). OK and Cancel are its own buttons:
// i3 gives a floating window no close button, so the touch screen had no way
// out (2026-10-06).
class SettingsPanel : public juce::Component
{
public:
	SettingsPanel (const juce::File& libraryFolder, std::function<void (const juce::File&)> onLibraryFolder);

	void resized() override;

private:
	void choose();
	void showFolder();
	void close();
	void ok();

	juce::Label libraryLabel { {}, "Stem library" };
	juce::Label libraryPath;
	juce::TextButton chooseButton { juce::String::fromUTF8 ("Choose\xe2\x80\xa6") };
	juce::Label libraryNote;
	juce::TextButton okButton { "OK" };
	juce::TextButton cancelButton { "Cancel" };

	std::function<void (const juce::File&)> onLibraryFolder;
	std::unique_ptr<juce::FileChooser> chooser;
	FolderChoice choice;
};
