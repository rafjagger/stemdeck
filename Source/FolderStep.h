#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

// Next / Prev on a deck: the set beside the loaded one in its own folder, in
// the order the library lists them. Sets of other folders in between (sorted
// by BPM, say) are passed over; at the folder's first or last set there is
// nothing -- no wrap into the next album.
namespace folderstep
{
	// `folders`: each set's folder, in the library's order; `current`: the
	// loaded set's index; `direction`: 1 next, -1 previous.
	std::optional<std::size_t> neighbour (const std::vector<std::string>& folders, std::size_t current, int direction);
}
