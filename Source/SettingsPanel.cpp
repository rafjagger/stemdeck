#include "SettingsPanel.h"
#include "Theme.h"

namespace
{
	const int barChoices[] = { 4, 8, 16, 32 };
	const int secondChoices[] = { 5, 10, 15, 20, 30 };
	constexpr int aboutTwentySecondsItem = 10000;   // a combo box item id must not be 0

	// The choices, and the stored value among them even when it is none of
	// them (set by hand in the settings file): shown, not lost on OK.
	template <size_t n>
	void fill (juce::ComboBox& box, const int (&choices)[n], int current)
	{
		std::vector<int> values (std::begin (choices), std::end (choices));
		if (current > 0 && std::find (values.begin(), values.end(), current) == values.end())
			values.push_back (current);
		std::sort (values.begin(), values.end());
		for (auto v : values)
			box.addItem (juce::String (v), v);
		box.setSelectedId (current, juce::dontSendNotification);
	}
}

SettingsPanel::SettingsPanel (const Values& values, Actions a)
	: fadeShown (values.autoDjFade),
	  actions (std::move (a)),
	  choice (values.libraryFolder.getFullPathName().toStdString())
{
	for (auto* heading : { &libraryLabel, &fadeLabel })
	{
		heading->setFont (juce::FontOptions (14.0f, juce::Font::bold));
		addAndMakeVisible (heading);
	}

	libraryPath.setColour (juce::Label::backgroundColourId, Theme::background);
	libraryPath.setColour (juce::Label::outlineColourId, Theme::outline);
	libraryPath.setMinimumHorizontalScale (0.6f);
	addAndMakeVisible (libraryPath);

	chooseButton.onClick = [this] { choose(); };
	addAndMakeVisible (chooseButton);

	libraryNote.setColour (juce::Label::textColourId, Theme::textDim);
	libraryNote.setText ("Artist / Album / sets below it. StemDeck remembers it.", juce::dontSendNotification);
	addAndMakeVisible (libraryNote);

	fadeBars.addItem (juce::String::fromUTF8 ("\xe2\x89\x88 20 s"), aboutTwentySecondsItem);
	fill (fadeBars, barChoices, values.autoDjFade.bars);
	if (values.autoDjFade.bars == AutoDj::aboutTwentySeconds)
		fadeBars.setSelectedId (aboutTwentySecondsItem, juce::dontSendNotification);
	fill (fadeSeconds, secondChoices, juce::roundToInt (values.autoDjFade.noGridSeconds));
	fadeBars.setTooltip ("Auto DJ overlaps the tracks for this many bars of the playing track -- or the whole bars "
						 "nearest 20 s, at least 4 -- and hands the stems over within them; applies to the next mix");
	fadeSeconds.setTooltip ("Tracks without a beat grid hand over in four steps over this many seconds; applies to the next mix");
	for (auto* c : { &fadeBars, &fadeSeconds })
		addAndMakeVisible (c);
	for (auto* unit : { &fadeBarsUnit, &fadeSecondsUnit })
	{
		unit->setColour (juce::Label::textColourId, Theme::textDim);
		addAndMakeVisible (unit);
	}

	// One deck: no Auto DJ; the phones decoder's mode where its row was.
	phonesModeShown = values.phonesMode;
	if (phonesModeShown)
	{
		for (auto* c : std::initializer_list<juce::Component*> { &fadeLabel, &fadeBars, &fadeSeconds, &fadeBarsUnit, &fadeSecondsUnit })
			c->setVisible (false);
		phonesLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
		addAndMakeVisible (phonesLabel);
		using spatial::PhonesMode;
		for (const auto mode : { PhonesMode::automatic, PhonesMode::on, PhonesMode::off })
			phonesMode.addItem (mode == PhonesMode::automatic ? "Automatic: off while Core is heard"
							   : mode == PhonesMode::on       ? "On while the phones ports are connected"
																: "Off",
								(int) mode + 1);
		phonesMode.setSelectedId ((int) *phonesModeShown + 1, juce::dontSendNotification);
		phonesMode.setTooltip ("StemDeck's own phones (phones_L/R) are for a laptop without the desk: on the rig the phones "
							   "come from Core. Automatic runs them only while Core is not heard.");
		addAndMakeVisible (phonesMode);
	}

	tipsWereShown = values.tipsShown;
	tipsButton.setToggleState (values.tipsShown, juce::dontSendNotification);
	addAndMakeVisible (tipsButton);

	audioButton.setEnabled (values.audioDeviceChoosable);
	audioButton.setTooltip (values.audioDeviceNote);
	audioButton.onClick = [this] { if (actions.onAudioDevice) actions.onAudioDevice(); };
	addAndMakeVisible (audioButton);

	okButton.onClick = [this] { ok(); };
	addAndMakeVisible (okButton);
	cancelButton.onClick = [this] { close(); };
	addAndMakeVisible (cancelButton);

	showFolder();
	setSize (520, 276);
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
	area.removeFromTop (8);

	fadeLabel.setBounds (area.removeFromTop (22));
	phonesLabel.setBounds (fadeLabel.getBounds());
	auto fade = area.removeFromTop (34);
	phonesMode.setBounds (fade.withWidth (juce::jmin (fade.getWidth(), 320)));
	fadeBars.setBounds (fade.removeFromLeft (80));
	fadeBarsUnit.setBounds (fade.removeFromLeft (60));
	fade.removeFromLeft (12);
	fadeSeconds.setBounds (fade.removeFromLeft (80));
	fadeSecondsUnit.setBounds (fade);
	area.removeFromTop (8);
	tipsButton.setBounds (area.removeFromTop (30));

	auto buttons = area.removeFromBottom (34);
	audioButton.setBounds (buttons.removeFromLeft (140));
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
	if (const auto folder = choice.toApply(); folder && actions.onLibraryFolder)
		actions.onLibraryFolder (juce::File (juce::String (*folder)));

	const auto barsItem = fadeBars.getSelectedId();
	const AutoDj::MixLength fade { barsItem == aboutTwentySecondsItem ? AutoDj::aboutTwentySeconds : barsItem,
								   (double) fadeSeconds.getSelectedId() };
	const auto changed = fade.bars != fadeShown.bars || ! juce::approximatelyEqual (fade.noGridSeconds, fadeShown.noGridSeconds);
	if (changed && actions.onAutoDjFade)
		actions.onAutoDjFade (fade);
	if (phonesModeShown && phonesMode.getSelectedId() > 0 && actions.onPhonesMode)
		if (const auto chosen = (spatial::PhonesMode) (phonesMode.getSelectedId() - 1); chosen != *phonesModeShown)
			actions.onPhonesMode (chosen);
	if (tipsButton.getToggleState() != tipsWereShown && actions.onTips)
		actions.onTips (tipsButton.getToggleState());
	close();
}

void SettingsPanel::close()
{
	if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
		dialog->exitModalState (0);
}
