#pragma once

#include <string>

// Where a stem set sits in the library: stems/Artist/Album/<files>. The
// artist is the first folder under the library folder, the album the second;
// deeper folders are added to the album ("Album / CD1"). A set lying straight
// in the library folder has neither, one level down only an artist.
// Pure: takes the set's folder relative to the library, '/'-separated.
struct LibraryPlace
{
	std::string artist;
	std::string album;
};

LibraryPlace libraryPlaceOf (const std::string& relativeFolder);
