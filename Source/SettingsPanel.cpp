#include "SettingsPanel.h"
#include "Theme.h"

SettingsPanel::SettingsPanel (const juce::File& libraryFolder, std::function<void (const juce::File&)> onFolder)
	: onLibraryFolder (std::move (onFolder)),
	  choice (libraryFolder.getFullPathName().toStdString())
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

	okButton.onClick = [this] { ok(); };
	addAndMakeVisible (okButton);
	cancelButton.onClick = [this] { close(); };
	addAndMakeVisible (cancelButton);

	showFolder();
	setSize (520, 156);
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

	auto buttons = area.removeFromBottom (34);
	okButton.setBounds (buttons.removeFromRight (110));
	buttons.removeFromRight (6);
	cancelButton.setBounds (buttons.removeFromRight (110));
}

void SettingsPanel::showFolder()
{
	libraryPath.setText (juce::String (choice.shown()), juce::dontSendNotification);
}

void SettingsPanel::choose()
{
	chooser = std::make_unique<juce::FileChooser> ("Stem library", juce::File (juce::String (choice.shown())));
	chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
						  [this] (const juce::FileChooser& fc)
	{
		const auto folder = fc.getResult();
		choice.picked (folder.isDirectory() ? folder.getFullPathName().toStdString() : std::string());
		showFolder();
	});
}

void SettingsPanel::ok()
{
	if (const auto folder = choice.toApply(); folder && onLibraryFolder)
		onLibraryFolder (juce::File (juce::String (*folder)));
	close();
}

void SettingsPanel::close()
{
	if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
		dialog->exitModalState (0);
}
