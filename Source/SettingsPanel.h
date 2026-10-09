#pragma once

#include <JuceHeader.h>

#include "AutoDj.h"
#include "FolderChoice.h"

// StemDeck's settings: where the stem library lives, how long the Auto DJ
// fades, and the way to the audio device. The folder is chosen, never typed
// -- the rig's touch screen has no keyboard to type with (2026-09-30). OK and
// Cancel are its own buttons: i3 gives a floating window no close button, so
// the touch screen had no way out (2026-10-06). The audio device applies as
// it is chosen, in its own panel, as before.
class SettingsPanel : public juce::Component
{
public:
	struct Values
	{
		juce::File libraryFolder;
		AutoDj::MixLength autoDjFade;
		bool audioDeviceChoosable = true;
		juce::String audioDeviceNote;   // its tooltip, why not
	};

	struct Actions
	{
		std::function<void (const juce::File&)> onLibraryFolder;
		std::function<void (AutoDj::MixLength)> onAutoDjFade;
		std::function<void()> onAudioDevice;
	};

	SettingsPanel (const Values& values, Actions actions);

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

	juce::Label fadeLabel { {}, "AutoDJ overlap" };
	juce::ComboBox fadeBars, fadeSeconds;
	juce::Label fadeBarsUnit { {}, "bars" }, fadeSecondsUnit { {}, "s without beat grid" };
	AutoDj::MixLength fadeShown;

	juce::TextButton audioButton { juce::String::fromUTF8 ("Audio device\xe2\x80\xa6") };
	juce::TextButton okButton { "OK" };
	juce::TextButton cancelButton { "Cancel" };

	Actions actions;
	std::unique_ptr<juce::FileChooser> chooser;
	FolderChoice choice;
};
