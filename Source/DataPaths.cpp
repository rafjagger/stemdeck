#include "DataPaths.h"

namespace
{
	std::string withoutTrailingSlash (std::string path)
	{
		while (path.size() > 1 && path.back() == '/')
			path.pop_back();
		return path;
	}
}

std::string dataFolder (const std::string& home, const char* xdgDataHome)
{
	const bool usable = xdgDataHome != nullptr && xdgDataHome[0] == '/';
	const auto base = usable ? withoutTrailingSlash (xdgDataHome)
							 : withoutTrailingSlash (home) + "/.local/share";
	return base + "/stemdeck";
}

std::string recordingsFolder (const std::string& home, const char* xdgDataHome)
{
	return dataFolder (home, xdgDataHome) + "/recordings";
}
