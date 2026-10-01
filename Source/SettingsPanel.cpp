#include "SettingsPanel.h"
#include "Theme.h"

SettingsPanel::SettingsPanel (const juce::File& libraryFolder, std::function<void (const juce::File&)> onFolder)
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

	showFolder();
	setSize (520, 110);
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
