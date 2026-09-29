#include "LibraryPath.h"

#include <sstream>
#include <vector>

LibraryPlace libraryPlaceOf (const std::string& relativeFolder)
{
	std::vector<std::string> levels;
	std::stringstream parts (relativeFolder);
	for (std::string level; std::getline (parts, level, '/');)
		if (! level.empty())
			levels.push_back (level);

	LibraryPlace place;
	if (! levels.empty())
		place.artist = levels[0];

	for (size_t i = 1; i < levels.size(); ++i)
		place.album += (i > 1 ? " / " : "") + levels[i];

	return place;
}
