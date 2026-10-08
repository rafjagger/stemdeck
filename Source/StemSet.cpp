#include "StemSet.h"
#include "LibraryPath.h"
#include "StemNames.h"
#include <map>

std::vector<StemSet> StemSet::scanFolder (const juce::File& folder, juce::AudioFormatManager& formats)
{
	struct Candidate
	{
		juce::String suffix;
		juce::File file;
	};

	std::map<juce::String, std::vector<Candidate>> groups;

	for (const auto& entry : juce::RangedDirectoryIterator (folder, true, formats.getWildcardForAllFormats(), juce::File::findFiles))
	{
		const auto file = entry.getFile();
		// The stem creator's originals are not sets (LibraryPath.h).
		const auto fileFolder = file.getParentDirectory();
		if (fileFolder != folder
			&& isIgnoredLibraryFolder (fileFolder.getRelativePathFrom (folder).replaceCharacter ('\\', '/').toStdString()))
			continue;
		std::string prefixText, suffixText;
		if (! splitStemName (file.getFileNameWithoutExtension().toStdString(), prefixText, suffixText))
			continue;
		const juce::String prefix (prefixText), suffix (suffixText);

		// Group per directory so equal names in different folders stay apart.
		const auto key = file.getParentDirectory().getFullPathName() + "/" + prefix;
		groups[key].push_back ({ suffix, file });
	}

	std::vector<StemSet> sets;

	for (auto& [key, candidates] : groups)
	{
		if ((int) candidates.size() != numStems)
			continue;

		std::sort (candidates.begin(), candidates.end(), [] (const Candidate& a, const Candidate& b)
		{
			return a.suffix.compareNatural (b.suffix) < 0;
		});

		StemSet set;
		set.name = key.fromLastOccurrenceOf ("/", false, false);

		for (int i = 0; i < numStems; ++i)
		{
			set.files[(size_t) i] = candidates[(size_t) i].file;
			set.stemNames[(size_t) i] = candidates[(size_t) i].suffix;
		}

		// Straight in the library folder JUCE's relative path is ".", not "".
		const auto setFolder = set.files[0].getParentDirectory();
		const auto relative = setFolder == folder ? juce::String() : setFolder.getRelativePathFrom (folder);
		set.folder = relative.replaceCharacter ('\\', '/');
		const auto place = libraryPlaceOf (set.folder.toStdString());
		set.artist = juce::String (place.artist);
		set.album = juce::String (place.album);

		if (std::unique_ptr<juce::AudioFormatReader> reader { formats.createReaderFor (set.files[0]) })
			set.lengthSeconds = (double) reader->lengthInSamples / reader->sampleRate;

		sets.push_back (std::move (set));
	}

	std::sort (sets.begin(), sets.end(), [] (const StemSet& a, const StemSet& b)
	{
		if (const auto order = a.artist.compareNatural (b.artist); order != 0) return order < 0;
		if (const auto order = a.album.compareNatural (b.album); order != 0)   return order < 0;
		return a.name.compareNatural (b.name) < 0;
	});

	return sets;
}
