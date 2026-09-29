#include "StemSet.h"
#include <map>

namespace
{
	// Splits "Artist - Title-001" into prefix "Artist - Title" and suffix "001".
	bool splitStemName (const juce::String& baseName, juce::String& prefix, juce::String& suffix)
	{
		const juce::String separators (" -_");
		int splitAt = -1;

		for (int i = baseName.length(); --i >= 0;)
		{
			if (separators.containsChar (baseName[i]))
			{
				splitAt = i;
				break;
			}
		}

		if (splitAt <= 0 || splitAt == baseName.length() - 1)
			return false;

		prefix = baseName.substring (0, splitAt).trimCharactersAtEnd (separators);
		suffix = baseName.substring (splitAt + 1);
		return prefix.isNotEmpty();
	}
}

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
		juce::String prefix, suffix;

		if (! splitStemName (file.getFileNameWithoutExtension(), prefix, suffix))
			continue;

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

		if (std::unique_ptr<juce::AudioFormatReader> reader { formats.createReaderFor (set.files[0]) })
			set.lengthSeconds = (double) reader->lengthInSamples / reader->sampleRate;

		sets.push_back (std::move (set));
	}

	std::sort (sets.begin(), sets.end(), [] (const StemSet& a, const StemSet& b)
	{
		return a.name.compareNatural (b.name) < 0;
	});

	return sets;
}
