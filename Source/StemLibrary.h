#pragma once

#include <JuceHeader.h>
#include "StemSet.h"
#include "TempoAnalysis.h"

// Table of the complete stem sets found in a folder. Sets are loaded with the
// deck buttons, by double-click (first deck that is not playing) or by
// dragging a row onto a deck.
class StemLibrary : public juce::Component,
					public juce::FileDragAndDropTarget,
					private juce::TableListBoxModel
{
public:
	explicit StemLibrary (juce::AudioFormatManager& formatManager);

	void setFolder (const juce::File& folder);
	const juce::File& getFolder() const { return folder; }

	// Selects and shows the set a file belongs to (after the creator made it).
	void selectSetWithFile (const juce::File& file);

	// The stem creator's line under the bar; empty hides it.
	void setCreatorStatus (const juce::String& text, bool canCancel, bool isError = false);

	// Looks up a set by the id carried in drag-and-drop descriptions.
	const StemSet* findSet (const juce::String& setId) const;

	void paint (juce::Graphics& g) override;
	void resized() override;

	std::function<void (const StemSet&, int deckIndex)> onLoadSet; // deckIndex -1: first free deck
	std::function<void (const juce::File&)> onFolderChanged;
	std::function<std::optional<BeatGrid> (const StemSet&)> lookUpBeatGrid; // for the BPM column
	std::function<void (const juce::Array<juce::File>&)> onCreateStems;       // stereo files, dropped or chosen
	std::function<void()> onCancelCreation;

	bool isInterestedInFileDrag (const juce::StringArray& files) override;
	void fileDragEnter (const juce::StringArray&, int, int) override { setDropHighlight (true); }
	void fileDragExit (const juce::StringArray&) override { setDropHighlight (false); }
	void filesDropped (const juce::StringArray& files, int, int) override;

	void analysisChanged() { table.repaint(); }

private:
	enum Columns { nameColumn = 1, bpmColumn, stemsColumn, lengthColumn, artistColumn, albumColumn };

	double bpmOf (const StemSet& set) const;

	int getNumRows() override;
	void paintRowBackground (juce::Graphics&, int row, int width, int height, bool selected) override;
	void paintCell (juce::Graphics&, int row, int columnId, int width, int height, bool selected) override;
	void cellDoubleClicked (int row, int columnId, const juce::MouseEvent&) override;
	void returnKeyPressed (int row) override;
	void sortOrderChanged (int columnId, bool forwards) override;
	juce::var getDragSourceDescription (const juce::SparseSet<int>& rows) override;

	static juce::String idFor (const StemSet& set) { return set.files[0].getFullPathName(); }
	void applyFilter();
	void loadSelected (int deckIndex);
	void chooseFolder();
	void chooseFilesForStems();
	void setDropHighlight (bool on);
	juce::Array<juce::File> separableFiles (const juce::StringArray& paths) const;
	void offerForStems (const juce::StringArray& paths);

	juce::AudioFormatManager& formatManager;
	juce::File folder;
	std::vector<StemSet> allSets;
	std::vector<const StemSet*> visibleSets;
	int sortColumn = artistColumn;
	bool sortForwards = true;

	juce::TextButton folderButton { "Ordner..." }, rescanButton { "Neu scannen" };
	juce::TextButton loadAButton { "Laden in A" }, loadBButton { "Laden in B" };
	juce::TextButton createButton { juce::String::fromUTF8 ("Stems erstellen\xe2\x80\xa6") }, cancelCreateButton { "Abbrechen" };
	juce::Label folderLabel, creatorLabel;
	bool dropHighlight = false;
	juce::TextEditor searchBox;
	juce::TableListBox table { "Sets", this };
	std::unique_ptr<juce::FileChooser> chooser;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StemLibrary)
};
