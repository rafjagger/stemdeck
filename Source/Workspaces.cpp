#include "Workspaces.h"

#include <cctype>
#include <cmath>

namespace
{
	// Where the leading number ends: the digits of "3:REAPER" or "12".
	std::size_t digitsEnd (const std::string& name)
	{
		std::size_t end = 0;
		while (end < name.size() && std::isdigit ((unsigned char) name[end]))
			++end;
		return end;
	}
}

std::string workspaceLabel (const std::string& name)
{
	const auto end = digitsEnd (name);
	return end > 0 && end < name.size() && name[end] == ':' ? name.substr (end + 1) : name;
}

int workspaceNumber (const std::string& name)
{
	const auto end = digitsEnd (name);
	return end == 0 ? 0 : std::stoi (name.substr (0, end));
}

SwitcherGeometry switcherGeometry (int windowWidth)
{
	const auto share = [windowWidth] (int atRigWidth) { return (int) std::lround (atRigWidth * windowWidth / 768.0); };
	return { share (6), share (30), share (2), share (80), share (40), share (170), share (40), share (4) };
}
