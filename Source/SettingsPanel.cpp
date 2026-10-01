#include "SettingsPanel.h"
#include "Theme.h"

SettingsPanel::SettingsPanel (const juce::File& libraryFolder, std::function<void (const juce::File&)> onFolder,
							  outputs::Mode outputMode, std::function<void (outputs::Mode)> onOutputMode)
	: onLibraryFolder (std::move (onFolder)),
	  current (libraryFolder)
{
	libraryLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
	addAndMakeVisible (libraryLabel);

	libraryPath.setColour (juce::Label::backgroundColourId, Theme::background);
	libraryPath.setColour (juce::Label::outlineColourId, Theme::outline);
	libraryPath.setMinimumHorizontalScale (0.6f);
	addAndMakeVisible (libraryPath);

	chooseButton.onClick = [this] { choose(); };
	addAndMakeVisible (chooseButton);

	libraryNote.setColour (juce::Label::textColourId, Theme::textDim);
	libraryNote.setText ("Artist / Album / sets below it. StemDeck remembers it.", juce::dontSendNotification);
	addAndMakeVisible (libraryNote);

	outputLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
	addAndMakeVisible (outputLabel);

	outputChoice.addItem (juce::String::fromUTF8 ("6\xc3\x97 stereo (internal routing)"), 1);
	outputChoice.addItem (juce::String::fromUTF8 ("8\xc3\x97 stereo (external routing)"), 2);
	outputChoice.setSelectedId (outputMode == outputs::Mode::Stems ? 2 : 1, juce::dontSendNotification);
	outputChoice.onChange = [this, onOutputMode = std::move (onOutputMode)]
	{
		onOutputMode (outputChoice.getSelectedId() == 2 ? outputs::Mode::Stems : outputs::Mode::Buses);
	};
	addAndMakeVisible (outputChoice);

	outputNote.setColour (juce::Label::textColourId, Theme::textDim);
	outputNote.setText (juce::String::fromUTF8 ("8\xc3\x97 stereo: every stem on its own pair, full level, for the A3 Mixer. Switching re-opens the output."),
						juce::dontSendNotification);
	outputNote.setMinimumHorizontalScale (0.6f);
	addAndMakeVisible (outputNote);

	showFolder();
	setSize (520, 210);
}

void SettingsPanel::resized()
{
	auto area = getLocalBounds().reduced (12);
	libraryLabel.setBounds (area.removeFromTop (22));
	auto row = area.removeFromTop (34);
	chooseButton.setBounds (row.removeFromRight (110));
	row.removeFromRight (6);
	libraryPath.setBounds (row);
	libraryNote.setBounds (area.removeFromTop (22));
	area.removeFromTop (12);
	outputLabel.setBounds (area.removeFromTop (22));
	outputChoice.setBounds (area.removeFromTop (34));
	outputNote.setBounds (area.removeFromTop (22));
}

void SettingsPanel::showFolder()
{
	libraryPath.setText (current.getFullPathName(), juce::dontSendNotification);
}

void SettingsPanel::choose()
{
	chooser = std::make_unique<juce::FileChooser> ("Stem library", current);
	chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
						  [this] (const juce::FileChooser& fc)
	{
		const auto folder = fc.getResult();
		if (! folder.isDirectory() || folder == current)
			return;
		current = folder;
		showFolder();
		if (onLibraryFolder)
			onLibraryFolder (folder);
	});
}
