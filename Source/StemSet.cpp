#include "StemSet.h"
#include "LibraryPath.h"
#include "StemNames.h"
#include <map>

std::vector<StemSet> StemSet::scanFolder (const juce::File& folder, juce::AudioFormatManager& formats)
{
	// Grouped per directory, so equal names in different folders stay apart.
	std::map<juce::String, std::vector<juce::File>> byFolder;

	for (const auto& entry : juce::RangedDirectoryIterator (folder, true, formats.getWildcardForAllFormats(), juce::File::findFiles))
	{
		const auto file = entry.getFile();
		// The stem creator's originals are not sets (LibraryPath.h).
		const auto fileFolder = file.getParentDirectory();
		if (fileFolder != folder
			&& isIgnoredLibraryFolder (fileFolder.getRelativePathFrom (folder).replaceCharacter ('\\', '/').toStdString()))
			continue;
		byFolder[fileFolder.getFullPathName()].push_back (file);
	}

	const auto naturalLess = [] (const std::string& a, const std::string& b)
	{
		return juce::String (a).compareNatural (juce::String (b)) < 0;
	};

	std::vector<StemSet> sets;

	for (const auto& [path, files] : byFolder)
	{
		std::vector<std::string> baseNames;
		for (const auto& file : files)
			baseNames.push_back (file.getFileNameWithoutExtension().toStdString());

		for (const auto& grouping : groupStemFiles (baseNames, naturalLess))
		{
			StemSet set;
			set.name = juce::String (grouping.name);
			for (size_t s = 0; s < grouping.slots.size(); ++s)
			{
				if (grouping.slots[s])
					set.files[s] = files[*grouping.slots[s]];
				set.stemNames[s] = juce::String (grouping.stemNames[s]);
			}

			// Straight in the library folder JUCE's relative path is ".", not "".
			const auto setFolder = juce::File (path);
			const auto relative = setFolder == folder ? juce::String() : setFolder.getRelativePathFrom (folder);
			set.folder = relative.replaceCharacter ('\\', '/');
			const auto place = libraryPlaceOf (set.folder.toStdString());
			set.artist = juce::String (place.artist);
			set.album = juce::String (place.album);

			if (std::unique_ptr<juce::AudioFormatReader> reader { formats.createReaderFor (set.firstFile()) })
				set.lengthSeconds = (double) reader->lengthInSamples / reader->sampleRate;

			sets.push_back (std::move (set));
		}
	}

	std::sort (sets.begin(), sets.end(), [] (const StemSet& a, const StemSet& b)
	{
		if (const auto order = a.artist.compareNatural (b.artist); order != 0) return order < 0;
		if (const auto order = a.album.compareNatural (b.album); order != 0)   return order < 0;
		return a.name.compareNatural (b.name) < 0;
	});

	return sets;
}
