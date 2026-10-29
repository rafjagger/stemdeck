#include "Workspaces.h"
#include "SpawnOutput.h"

#include <juce_core/juce_core.h>

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

I3Runner systemI3Runner()
{
	// Every 500 ms from the panel's poller: spawned, never forked (SpawnOutput.h).
	return [] (const std::vector<std::string>& argv) { return spawnForOutput (argv, 1000); };
}

void goToWorkspace (const I3Runner& i3, int number)
{
	i3 ({ "i3-msg", "-q", "workspace number " + std::to_string (number) });
}

std::vector<WorkspaceEntry> listWorkspaces (const I3Runner& i3)
{
	// Held in a named var: getArray() points into it, and a temporary's list
	// was gone before the loop read it (SIGSEGV on the rig, 2026-09-30).
	const auto workspaces = juce::JSON::parse (juce::String (i3 ({ "i3-msg", "-t", "get_workspaces" })));
	const auto* list = workspaces.getArray();
	if (list == nullptr)
		return {};

	std::vector<WorkspaceEntry> entries;
	for (const auto& workspace : *list)
	{
		const auto name = workspace["name"].toString().toStdString();
		if (const auto number = workspaceNumber (name); number > 0)
			entries.push_back ({ number, workspaceLabel (name), (bool) workspace["focused"] });
	}
	return entries;
}
