// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "StemCreatorLink.h"

#include "StemJob.h"

StemCreatorLink::StemCreatorLink (StemLibrary& l, StemCreator& c, juce::PropertiesFile& s, juce::Component& parent)
	: library (l), creator (c), settings (s), dialogParent (parent)
{
	library.onFolderChanged = [this] (const juce::File& folder)
	{
		settings.setValue ("stemFolder", folder.getFullPathName());
		creator.setLibraryFolder (folder);
	};
	library.onCreateStems = [this] (const juce::Array<juce::File>& files) { createStems (files); };
	library.onCancelCreation = [this] { creator.cancelRunning(); };
	creator.onSetCreated = [this] (const juce::File& firstStem)
	{
		library.setFolder (library.getFolder());
		library.selectSetWithFile (firstStem);
	};
	if (const auto venv = settings.getValue ("separatorVenv"); juce::File::isAbsolutePath (venv))
		creator.setVenv (venv);
	// All cores but the audio CPU, unless the settings say fewer.
	if (const auto cores = settings.getIntValue ("separatorCores"); cores > 0)
		creator.setMaxCores (cores);

	// Last used folder, otherwise ./stems next to where the app was started.
	// A saved folder that is gone (the checkout moved) falls back too, or the
	// library stays empty with nothing to say why.
	const auto savedFolder = settings.getValue ("stemFolder");
	const juce::File saved (savedFolder.isNotEmpty() && juce::File::isAbsolutePath (savedFolder) ? savedFolder : juce::String());
	library.setFolder (saved.isDirectory() ? saved
										   : juce::File::getCurrentWorkingDirectory().getChildFile ("stems"));
	creator.setLibraryFolder (library.getFolder());
}

// One question for the whole batch: the folder it goes to, below the library.
// Every track lands there, named after its file.
void StemCreatorLink::createStems (const juce::Array<juce::File>& files)
{
	if (! creator.isInstalled())
	{
		juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Create stems",
			"The stem separator (Demucs) is not installed:\n" + creator.venv().getChildFile ("bin/demucs").getFullPathName()
			+ juce::String (" is missing.\n\nSet it up once (about 1 GB):\n"
							"  sudo apt install ffmpeg python3-venv\n"
							"  tools/setup-separator.sh"));
		return;
	}

	std::vector<std::string> paths;
	for (const auto& f : files)
		paths.push_back (f.getFullPathName().toStdString());
	askTargetFolder (files, juce::String (suggestTargetFolder (paths)));
}

void StemCreatorLink::askTargetFolder (const juce::Array<juce::File>& files, const juce::String& preset)
{
	const auto count = files.size() == 1 ? files.getFirst().getFileNameWithoutExtension()
										 : juce::String (files.size()) + " tracks";
	const auto libraryFolder = library.getFolder();

	stemDialog = std::make_unique<juce::AlertWindow> ("Create stems",
		count + juce::String ("\n\nTarget folder in ") + libraryFolder.getFullPathName()
			  + juce::String::fromUTF8 (":\n(Artist/Album \xe2\x80\x93 empty: straight into the library folder)"),
		juce::MessageBoxIconType::NoIcon, &dialogParent);
	stemDialog->addTextEditor ("folder", preset, "Target folder");
	stemDialog->addButton ("Create", 1, juce::KeyPress (juce::KeyPress::returnKey));
	stemDialog->addButton (juce::String::fromUTF8 ("Browse\xe2\x80\xa6"), 2);
	stemDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
	stemDialog->enterModalState (true, juce::ModalCallbackFunction::create ([this, files, libraryFolder] (int result)
	{
		const auto typed = stemDialog->getTextEditorContents ("folder").trim();
		stemDialog.reset();

		if (result == 1)
		{
			const auto folder = juce::String (sanitiseFolder (typed.toStdString()));
			for (const auto& f : files)
				creator.add (f, folder, f.getFileNameWithoutExtension());
		}
		else if (result == 2)
		{
			const auto start = libraryFolder.getChildFile (juce::String (sanitiseFolder (typed.toStdString())));
			folderChooser = std::make_unique<juce::FileChooser> ("Target folder", start.isDirectory() ? start : libraryFolder);
			folderChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
				[this, files, libraryFolder, typed] (const juce::FileChooser& fc)
				{
					const auto chosen = fc.getResult();
					if (chosen == juce::File())
						return askTargetFolder (files, typed);   // closed: back to the question
					if (chosen != libraryFolder && ! chosen.isAChildOf (libraryFolder))
					{
						juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Create stems",
							juce::String ("The target folder has to be inside the library folder:\n") + libraryFolder.getFullPathName());
						return askTargetFolder (files, typed);
					}
					askTargetFolder (files, chosen == libraryFolder ? juce::String() : chosen.getRelativePathFrom (libraryFolder));
				});
		}
	}));
}

void StemCreatorLink::showStatus()
{
	// No pause while a deck plays: the separator keeps off the audio CPU.
	const auto status = creator.status();
	juce::String text;
	if (status.running)
	{
		text = "Stems: " + status.track + "  " + juce::String (juce::roundToInt (status.progress * 100.0)) + " %";
		if (status.waiting > 0)
			text << "  +" << status.waiting << " waiting";
	}
	else if (status.waitingForOtherInstance)
		text = "Stems: " + juce::String (status.waiting) + " waiting for the other StemDeck to finish";
	else if (status.lastError.isNotEmpty())
		text = "Stems: failed: " + status.lastError;

	library.setCreatorStatus (text, status.running, ! status.running && status.lastError.isNotEmpty());
}

void StemCreatorLink::gather (std::vector<QueuedStemJob>& jobs) const
{
	for (const auto& job : creator.pendingJobs())
		jobs.push_back ({ juce::String (job.input), juce::String (job.folder), juce::String (job.track) });
}

void StemCreatorLink::restore (const std::vector<QueuedStemJob>& jobs)
{
	for (const auto& job : jobs)
		if (juce::File (job.input).existsAsFile())
			creator.add (juce::File (job.input), job.folder, job.track);
}
