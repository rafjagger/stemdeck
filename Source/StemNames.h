#pragma once

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <vector>

// How stem files are named, both ways. Pure: no JUCE, testable.
//
// Files in one folder whose names share the part before the last space, '-'
// or '_' (the prefix) group into a set (groupStemFiles below); the part after
// it (the suffix) puts them on slots 1-4. One exception: "Title - 1 - drums" -- a number and a
// name, each after a dash -- splits before the number, so the stem creator's
// "Title - 1 - drums" ... "Title - 4 - vocals" are one set in bus order.
// ("Title - 1 drums", without the second dash, still gives every stem its own
// prefix and no set at all.)

// Splits "Artist - Title-001" into prefix "Artist - Title" and suffix "001",
// and "Title - 2 - bass" into "Title" and "2 - bass".
// False when there is no separator to split at.
bool splitStemName (const std::string& baseName, std::string& prefix, std::string& suffix);

// The file name of stem `stem` (0 drums, 1 bass, 2 other, 3 vocals) of a track.
std::string stemFileName (const std::string& track, int stem, const std::string& extension);

// A name safe as a file or folder name that also survives splitStemName:
// no '/', '\\' or NUL, no leading '.', no trailing space, '-' or '_'.
// Empty after cleaning: `fallback`.
std::string sanitiseName (const std::string& name, const std::string& fallback);

// A set of one to four stereo files (2026-10-29). The files of one folder
// group by prefix (splitStemName); then:
//   - four files of one prefix are a set, ordered by suffix onto slots 1-4,
//     as they always were ("Title-001" ... "-004", "DUB", "KICK", ...);
//   - two or three files whose suffixes all name a slot (numberedSlot), each
//     a different one, are a set with file n on slot n, the rest empty;
//   - every other file is a set of its own on slot 1, named by its whole
//     name: a lone stereo file, an album's "01 - Intro", a group of five.
// A slot number is one digit without a leading zero, so an album's "01",
// "02" never make a set of two.
struct StemGrouping
{
	std::string name;
	std::array<std::optional<size_t>, 4> slots;   // an index into the names given
	std::array<std::string, 4> stemNames;         // the suffix; a lone file's whole name
};

// The sets among the base names (no extension) of one folder's audio files.
// `lessSuffix` orders a four-file group (the caller's natural sort).
std::vector<StemGrouping> groupStemFiles (const std::vector<std::string>& baseNames,
										  const std::function<bool (const std::string&, const std::string&)>& lessSuffix);

// The slot (0-3) a stem creator's suffix names: "2 - bass" and "2.bass" are
// slot 1. Nothing for "02 - bass", "5 - x", a bare "2" or "DUB".
std::optional<int> numberedSlot (const std::string& suffix);

// The loaded slots by number, as the deck and the library show a set:
// "1 2 3 4", "1", "2 4".
std::string loadedSlotsText (const std::array<bool, 4>& loaded);
