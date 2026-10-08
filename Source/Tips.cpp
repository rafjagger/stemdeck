#include "Tips.h"

namespace tips
{
	bool shown (const std::string& stored)
	{
		if (stored.empty())
			return shownByDefault;
		return stored != "0";
	}

	std::string stored (bool shown)
	{
		return shown ? "1" : "0";
	}
}
