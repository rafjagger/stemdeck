#include "FolderStep.h"

std::optional<std::size_t> folderstep::neighbour (const std::vector<std::string>& folders, std::size_t current, int direction)
{
	if (current >= folders.size() || direction == 0)
		return std::nullopt;

	const auto& folder = folders[current];
	const auto step = direction > 0 ? 1 : -1;

	for (auto i = (long long) current + step; i >= 0 && i < (long long) folders.size(); i += step)
		if (folders[(std::size_t) i] == folder)
			return (std::size_t) i;

	return std::nullopt;
}
