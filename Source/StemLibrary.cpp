#include "StemLibrary.h"
#include "Theme.h"
#include "Waveforms.h"

StemLibrary::StemLibrary (juce::AudioFormatManager& fm) : formatManager (fm)
{
	folderButton.onClick = [this] { chooseFolder(); };
	rescanButton.onClick = [this] { setFolder (folder); };
	loadAButton.onClick = [this] { loadSelected (0); };
	loadBButton.onClick = [this] { loadSelected (1); };
	loadAButton.setColour (juce::TextButton::textColourOffId, Theme::deck (0));
	loadBButton.setColour (juce::TextButton::textColourOffId, Theme::deck (1));

	for (auto* b : { &folderButton, &rescanButton, &loadAButton, &loadBButton })
	{
		b->setMouseClickGrabsKeyboardFocus (false);
		addAndMakeVisible (b);
	}

	folderLabel.setColour (juce::Label::textColourId, Theme::textDim);
	folderLabel.setMinimumHorizontalScale (0.6f);
	addAndMakeVisible (folderLabel);

	searchBox.setTextToShowWhenEmpty ("Suchen...", Theme::textDim);
	searchBox.setColour (juce::TextEditor::backgroundColourId, Theme::background);
	searchBox.setColour (juce::TextEditor::outlineColourId, Theme::outline);
	searchBox.onTextChange = [this] { applyFilter(); };
	searchBox.onReturnKey = [this] { loadSelected (-1); };
	searchBox.onEscapeKey = [this] { searchBox.clear(); applyFilter(); };
	addAndMakeVisible (searchBox);

	auto& header = table.getHeader();
	// stems/Artist/Album/<sets>: the folders are the first two columns, and
	// the table is sorted artist, album, set to begin with.
	header.addColumn ("Artist", artistColumn, 180, 80);
	header.addColumn ("Album", albumColumn, 180, 80);
	header.addColumn ("Set", nameColumn, 280, 120);
	header.addColumn ("BPM", bpmColumn, 70, 50);
	header.addColumn ("Stems", stemsColumn, 200, 80);
	header.addColumn (juce::String::fromUTF8 ("L\xc3\xa4nge"), lengthColumn, 70, 50);
	header.setSortColumnId (artistColumn, true);
	header.setColour (juce::TableHeaderComponent::backgroundColourId, Theme::panelRaised);
	header.setColour (juce::TableHeaderComponent::textColourId, Theme::textDim);

	table.setRowHeight (24);
	table.setMultipleSelectionEnabled (false);
	table.setColour (juce::ListBox::backgroundColourId, Theme::background);
	addAndMakeVisible (table);
}

void StemLibrary::setFolder (const juce::File& newFolder)
{
	folder = newFolder;
	allSets = folder.isDirectory() ? StemSet::scanFolder (folder, formatManager) : std::vector<StemSet>();
	folderLabel.setText (folder.getFullPathName() + "  -  " + juce::String ((int) allSets.size()) + " Sets",
						 juce::dontSendNotification);
	applyFilter();

	if (onFolderChanged)
		onFolderChanged (folder);
}

const StemSet* StemLibrary::findSet (const juce::String& setId) const
{
	for (const auto& set : allSets)
		if (idFor (set) == setId)
			return &set;

	return nullptr;
}

void StemLibrary::applyFilter()
{
	const auto words = juce::StringArray::fromTokens (searchBox.getText(), true);
	visibleSets.clear();

	for (const auto& set : allSets)
		if (std::all_of (words.begin(), words.end(), [&set] (const juce::String& w)
			{
				return set.name.containsIgnoreCase (w) || set.artist.containsIgnoreCase (w) || set.album.containsIgnoreCase (w);
			}))
			visibleSets.push_back (&set);

	std::stable_sort (visibleSets.begin(), visibleSets.end(), [this] (const StemSet* a, const StemSet* b)
	{
		int order = 0;

		// Artist and album sort down through the levels below them, so an
		// artist's albums and an album's sets stay together and in order.
		switch (sortColumn)
		{
			case lengthColumn: order = a->lengthSeconds < b->lengthSeconds ? -1 : (a->lengthSeconds > b->lengthSeconds ? 1 : 0); break;
			case bpmColumn:    order = bpmOf (*a) < bpmOf (*b) ? -1 : (bpmOf (*a) > bpmOf (*b) ? 1 : 0); break;
			case artistColumn: order = a->artist.compareNatural (b->artist);
							   if (order == 0) order = a->album.compareNatural (b->album);
							   if (order == 0) order = a->name.compareNatural (b->name);
							   break;
			case albumColumn:  order = a->album.compareNatural (b->album);
							   if (order == 0) order = a->name.compareNatural (b->name);
							   break;
			default:           order = a->name.compareNatural (b->name); break;
		}

		return sortForwards ? order < 0 : order > 0;
	});

	table.updateContent();
	table.selectRow (0);
	table.repaint();
}

double StemLibrary::bpmOf (const StemSet& set) const
{
	if (lookUpBeatGrid)
		if (const auto grid = lookUpBeatGrid (set))
			return grid->bpm;

	return 0.0;
}

void StemLibrary::sortOrderChanged (int columnId, bool forwards)
{
	sortColumn = columnId;
	sortForwards = forwards;
	applyFilter();
}

void StemLibrary::loadSelected (int deckIndex)
{
	const auto row = table.getSelectedRow();

	if (juce::isPositiveAndBelow (row, (int) visibleSets.size()) && onLoadSet)
		onLoadSet (*visibleSets[(size_t) row], deckIndex);
}

void StemLibrary::chooseFolder()
{
	chooser = std::make_unique<juce::FileChooser> ("Stem-Ordner", folder);
	chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
						  [this] (const juce::FileChooser& fc)
	{
		if (fc.getResult().isDirectory())
			setFolder (fc.getResult());
	});
}

//==============================================================================
int StemLibrary::getNumRows()
{
	return (int) visibleSets.size();
}

void StemLibrary::paintRowBackground (juce::Graphics& g, int row, int, int, bool selected)
{
	if (selected)
		g.fillAll (juce::Colour (0xff34506b));
	else if (row % 2 == 1)
		g.fillAll (Theme::panel);
}

void StemLibrary::paintCell (juce::Graphics& g, int row, int columnId, int width, int height, bool)
{
	if (! juce::isPositiveAndBelow (row, (int) visibleSets.size()))
		return;

	const auto& set = *visibleSets[(size_t) row];
	juce::String text;

	switch (columnId)
	{
		case nameColumn:   text = set.name; break;
		case stemsColumn:  { juce::StringArray n; for (const auto& s : set.stemNames) n.add (s); text = n.joinIntoString (" / "); break; }
		case bpmColumn:    { const auto bpm = bpmOf (set); text = bpm > 0.0 ? juce::String (bpm, 2) : juce::String(); break; }
		case lengthColumn: text = Theme::formatTime (set.lengthSeconds).upToLastOccurrenceOf (".", false, false); break;
		case artistColumn: text = set.artist; break;
		case albumColumn:  text = set.album; break;
		default: break;
	}

	g.setColour (columnId == nameColumn ? Theme::text : Theme::textDim);
	g.setFont (juce::FontOptions (14.0f));
	g.drawText (text, 6, 0, width - 12, height, juce::Justification::centredLeft, true);
}

void StemLibrary::cellDoubleClicked (int row, int, const juce::MouseEvent&)
{
	table.selectRow (row);
	loadSelected (-1);
}

void StemLibrary::returnKeyPressed (int)
{
	loadSelected (-1);
}

juce::var StemLibrary::getDragSourceDescription (const juce::SparseSet<int>& rows)
{
	if (rows.size() == 1 && juce::isPositiveAndBelow (rows[0], (int) visibleSets.size()))
		return juce::String (StemSetDropTarget::prefix) + idFor (*visibleSets[(size_t) rows[0]]);

	return {};
}

//==============================================================================
void StemLibrary::paint (juce::Graphics& g)
{
	g.setColour (Theme::panel);
	g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f);
}

void StemLibrary::resized()
{
	auto area = getLocalBounds().reduced (10);

	auto bar = area.removeFromTop (30);
	searchBox.setBounds (bar.removeFromLeft (280));
	bar.removeFromLeft (10);
	loadAButton.setBounds (bar.removeFromLeft (100));
	bar.removeFromLeft (6);
	loadBButton.setBounds (bar.removeFromLeft (100));
	bar.removeFromLeft (16);
	rescanButton.setBounds (bar.removeFromRight (110));
	bar.removeFromRight (6);
	folderButton.setBounds (bar.removeFromRight (90));
	bar.removeFromRight (10);
	folderLabel.setBounds (bar);

	area.removeFromTop (8);
	table.setBounds (area);
}
