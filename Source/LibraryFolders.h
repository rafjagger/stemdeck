#pragma once

#include <cstddef>
#include <string>
#include <vector>

// The library's folder tree and what a chosen folder shows. Pure: paths are
// relative to the library folder, '/'-separated; "" is the library folder
// itself, which shows everything.
//
// The tree is made from the sets' folders and their parents, not from the
// disk: a folder without a set below it (the creator's originals, a config
// folder) is not a place to look for one.
namespace libraryfolders
{
	// A set in `setFolder` is shown with `chosen` chosen: it is that folder
	// or below it.
	bool isWithin (const std::string& setFolder, const std::string& chosen);
	std::size_t countWithin (const std::vector<std::string>& setFolders, const std::string& chosen);

	// Every folder holding a set, with its parents; each parent before its
	// children, siblings by name (case aside), no duplicates, no root.
	std::vector<std::string> folderTree (const std::vector<std::string>& setFolders);

	// `chosen` while it is in the tree, the root otherwise (a session's folder
	// that was renamed or emptied).
	std::string validChoice (const std::string& chosen, const std::vector<std::string>& tree);

	std::string nameOf (const std::string& folder);
	std::string parentOf (const std::string& folder);
}
