#include "OscTruthFile.h"

#include <juce_core/juce_core.h>

namespace osctruth
{
	std::vector<Listener> readListeners (const std::string& path, std::string& error)
	{
		const juce::File file (path);
		if (! file.existsAsFile())
		{
			error = "no a3-osc.json at " + path;
			return {};
		}

		juce::var truth;
		if (juce::JSON::parse (file.loadFileAsString(), truth).failed())
		{
			error = path + " does not parse";
			return {};
		}

		std::vector<Listener> listeners;
		if (const auto* entries = truth["listeners"].getArray())
			for (const auto& entry : *entries)
				listeners.push_back ({ entry["program"].toString().toStdString(),
									   entry["role"].toString().toStdString(),
									   entry["host"].toString().toStdString(),
									   (int) entry["port"] });
		error.clear();
		return listeners;
	}
}
