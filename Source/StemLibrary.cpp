#include "StemLibrary.h"
#include "Theme.h"
#include "Waveforms.h"
#include "StemJob.h"

StemLibrary::StemLibrary (juce::AudioFormatManager& fm) : formatManager (fm)
{
	rescanButton.onClick = [this] { setFolder (folder); };
	loadAButton.onClick = [this] { loadSelected (0); };
	loadBButton.onClick = [this] { loadSelected (1); };
	createButton.onClick = [this] { chooseFilesForStems(); };
	cancelCreateButton.onClick = [this] { if (onCancelCreation) onCancelCreation(); };
	loadAButton.setColour (juce::TextButton::textColourOffId, Theme::deck (0));
	loadBButton.setColour (juce::TextButton::textColourOffId, Theme::deck (1));

	for (auto* b : { &rescanButton, &loadAButton, &loadBButton, &createButton, &cancelCreateButton })
	{
		b->setMouseClickGrabsKeyboardFocus (false);
		addAndMakeVisible (b);
	}

	folderLabel.setColour (juce::Label::textColourId, Theme::textDim);
	folderLabel.setMinimumHorizontalScale (0.6f);
	addAndMakeVisible (folderLabel);

	creatorLabel.setColour (juce::Label::textColourId, Theme::textDim);
	creatorLabel.setMinimumHorizontalScale (0.7f);
	addChildComponent (creatorLabel);
	cancelCreateButton.setVisible (false);

	searchBox.setTextToShowWhenEmpty ("Search...", Theme::textDim);
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
	header.addColumn (juce::String ("Length"), lengthColumn, 70, 50);
	// Into the window's width rather than past its right edge (768 px on the rig).
	header.setStretchToFitActive (true);
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
	folderLabel.setText (folder.getFullPathName() + "  -  " + juce::String ((int) allSets.size()) + " sets",
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

void StemLibrary::selectRelative (int delta)
{
	if (visibleSets.empty())
		return;
	const auto row = juce::jlimit (0, (int) visibleSets.size() - 1, juce::jmax (0, table.getSelectedRow()) + delta);
	table.selectRow (row);
	table.scrollToEnsureRowIsOnscreen (row);
}

const StemSet* StemLibrary::selectedSet() const
{
	const auto row = table.getSelectedRow();
	return row >= 0 && row < (int) visibleSets.size() ? visibleSets[(size_t) row] : nullptr;
}

const StemSet* StemLibrary::randomVisibleSet (const std::set<juce::String>& played) const
{
	std::vector<const StemSet*> fresh;
	for (const auto* set : visibleSets)
		if (played.count (idFor (*set)) == 0)
			fresh.push_back (set);

	const auto& from = fresh.empty() ? visibleSets : fresh;
	if (from.empty())
		return nullptr;
	return from[(size_t) juce::Random::getSystemRandom().nextInt ((int) from.size())];
}

juce::String StemLibrary::storedId (const juce::String& setId) const
{
	const juce::File file (setId);
	return setId.isNotEmpty() && file.isAChildOf (folder) ? file.getRelativePathFrom (folder) : setId;
}

juce::String StemLibrary::idFromStored (const juce::String& stored) const
{
	return stored.isEmpty() || juce::File::isAbsolutePath (stored) ? stored : folder.getChildFile (stored).getFullPathName();
}

void StemLibrary::saveState (LibrarySession& state) const
{
	state.sortColumn = sortColumn;
	state.sortForwards = sortForwards;
	state.search = searchBox.getText();
	const auto row = table.getSelectedRow();
	state.selectedSetId = row >= 0 && row < (int) visibleSets.size() ? storedId (idFor (*visibleSets[(size_t) row])) : juce::String();
}

void StemLibrary::restoreState (const LibrarySession& state)
{
	if (state.sortColumn > 0)
		table.getHeader().setSortColumnId (state.sortColumn, state.sortForwards);   // sorts through sortOrderChanged
	searchBox.setText (state.search, true);                                          // filters through onTextChange

	for (int row = 0; row < (int) visibleSets.size(); ++row)
		if (idFor (*visibleSets[(size_t) row]) == idFromStored (state.selectedSetId))
		{
			table.selectRow (row);
			table.scrollToEnsureRowIsOnscreen (row);
			break;
		}
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

	if (dropHighlight)
	{
		g.setColour (Theme::deck (0));
		g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (3.0f), 6.0f, 2.0f);
	}
}

void StemLibrary::resized()
{
	auto area = getLocalBounds().reduced (10);

	// Two rows where one does not hold them all (the rig's 768 px screen): the
	// search and the load keys, then where the library is, creating and
	// rescanning. The folder itself is chosen in Settings (2026-09-30).
	const auto twoRows = getWidth() < 1000;

	auto bar = area.removeFromTop (30);
	auto folderBar = twoRows ? (area.removeFromTop (6), area.removeFromTop (30)) : bar;

	searchBox.setBounds (bar.removeFromLeft (twoRows ? bar.getWidth() - 2 * 100 - 2 * 6 : 280));
	bar.removeFromLeft (twoRows ? 6 : 10);
	loadAButton.setBounds (bar.removeFromLeft (100));
	bar.removeFromLeft (6);
	loadBButton.setBounds (bar.removeFromLeft (100));
	bar.removeFromLeft (16);
	if (! twoRows)
		folderBar = bar;

	rescanButton.setBounds (folderBar.removeFromRight (110));
	folderBar.removeFromRight (6);
	createButton.setBounds (folderBar.removeFromRight (130));
	folderBar.removeFromRight (10);
	folderLabel.setBounds (folderBar);

	if (creatorLabel.isVisible())
	{
		area.removeFromTop (6);
		auto strip = area.removeFromTop (24);
		if (cancelCreateButton.isVisible())
			cancelCreateButton.setBounds (strip.removeFromRight (100));
		creatorLabel.setBounds (strip);
	}

	area.removeFromTop (8);
	table.setBounds (area);
}

void StemLibrary::selectSetWithFile (const juce::File& file)
{
	for (int row = 0; row < (int) visibleSets.size(); ++row)
		for (const auto& f : visibleSets[(size_t) row]->files)
			if (f == file)
			{
				table.selectRow (row);
				table.scrollToEnsureRowIsOnscreen (row);
				return;
			}
}

void StemLibrary::setCreatorStatus (const juce::String& text, bool canCancel, bool isError)
{
	const auto show = text.isNotEmpty();
	creatorLabel.setText (text, juce::dontSendNotification);
	creatorLabel.setColour (juce::Label::textColourId, isError ? Theme::mute : Theme::textDim);
	if (show == creatorLabel.isVisible() && canCancel == cancelCreateButton.isVisible())
		return;
	creatorLabel.setVisible (show);
	cancelCreateButton.setVisible (show && canCancel);
	resized();
}

juce::Array<juce::File> StemLibrary::separableFiles (const juce::StringArray& paths) const
{
	const auto offered = [this] (const juce::File& f)
	{
		return shouldOfferForSeparation (f.getFullPathName().toStdString(), folder.getFullPathName().toStdString());
	};

	juce::Array<juce::File> found;
	for (const auto& path : paths)
	{
		const juce::File f (path);
		if (f.isDirectory())
		{
			for (const auto& entry : juce::RangedDirectoryIterator (f, true, "*", juce::File::findFiles))
				if (offered (entry.getFile()))
					found.add (entry.getFile());
		}
		else if (offered (f))
			found.add (f);
	}
	found.sort();
	return found;
}

bool StemLibrary::isInterestedInFileDrag (const juce::StringArray& files)
{
	for (const auto& path : files)
		if (juce::File (path).isDirectory() || isSeparableAudioFile (juce::File (path).getFileName().toStdString()))
			return true;
	return false;
}

void StemLibrary::filesDropped (const juce::StringArray& files, int, int)
{
	setDropHighlight (false);
	offerForStems (files);
}

void StemLibrary::offerForStems (const juce::StringArray& paths)
{
	if (paths.isEmpty())
		return;

	if (const auto found = separableFiles (paths); ! found.isEmpty())
	{
		if (onCreateStems)
			onCreateStems (found);
		return;
	}

	juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Create stems",
		juce::String ("No stereo file to separate.\n\n"
					  "Files already in the library folder are not separated: put the originals outside\n")
		+ folder.getFullPathName()
		+ "\nand drag them in from there.\n\nFormats: FLAC, WAV, MP3, AIFF, OGG, M4A, Opus.");
}

void StemLibrary::setDropHighlight (bool on)
{
	dropHighlight = on;
	repaint();
}

void StemLibrary::chooseFilesForStems()
{
	chooser = std::make_unique<juce::FileChooser> ("Stereo-Dateien in Stems zerlegen",
		juce::File::getSpecialLocation (juce::File::userMusicDirectory),
		"*.flac;*.wav;*.mp3;*.aiff;*.aif;*.ogg;*.m4a;*.opus");
	chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
							  | juce::FileBrowserComponent::canSelectMultipleItems,
						  [this] (const juce::FileChooser& fc)
						  {
							  juce::StringArray paths;
							  for (const auto& f : fc.getResults())
								  paths.add (f.getFullPathName());
							  offerForStems (paths);
						  });
}
