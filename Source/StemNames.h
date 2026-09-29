#pragma once

#include <string>

// How stem files are named, both ways. Pure: no JUCE, testable.
//
// A set is four files in one folder whose names share the part before the
// last space, '-' or '_' (the prefix); the part after it (the suffix) orders
// them onto buses 1-4. One exception: "Title - 1 - drums" -- a number and a
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
