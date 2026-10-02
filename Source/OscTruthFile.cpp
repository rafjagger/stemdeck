#include "OscTruthFile.h"
#include "TruthKeeper.h"

#include <cstdlib>

#include <juce_core/juce_core.h>

namespace osctruth
{
	namespace
	{
		juce::var parsed (const std::string& path, std::string& error)
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
			error.clear();
			return truth;
		}
	}

	std::vector<Listener> readListeners (const std::string& path, std::string& error)
	{
		const auto truth = parsed (path, error);
		if (! error.empty())
			return {};

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

	std::map<std::string, std::string> readHosts (const std::string& path, std::string& error)
	{
		const auto truth = parsed (path, error);
		std::map<std::string, std::string> hosts;
		if (const auto* object = truth["hosts"].getDynamicObject())
			for (const auto& property : object->getProperties())
				hosts[property.name.toString().toStdString()] = property.value.toString().toStdString();
		return hosts;
	}

	remote::Words readRemoteWords (const std::string& path, std::string& error)
	{
		const auto truth = parsed (path, error);
		if (! error.empty())
			return {};
		const auto patternOf = [&truth, &error] (const char* key) {
			const auto pattern = truth["addresses"][key]["pattern"].toString().toStdString();
			if (pattern.empty())
				error = std::string ("a3-osc.json has no ") + key;
			return pattern;
		};
		return { patternOf ("stemdeck.bus"), patternOf ("stemdeck.buses"), patternOf ("stemdeck.recall"),
				 patternOf ("vu"), patternOf ("device.hello") };
	}

	std::string unusableTruth (const std::string& text)
	{
		juce::var truth;
		if (juce::JSON::parse (juce::String (text), truth).failed())
			return "not JSON";
		if (truth["addresses"]["stemdeck.bus"]["pattern"].toString().isEmpty())
			return "it has no stemdeck.bus";
		return {};
	}

	std::string liveTruthPath()
	{
		const auto cache = truthkeeper::cachePath (juce::File::getSpecialLocation (
			juce::File::userHomeDirectory).getFullPathName().toStdString());
		const juce::File cached (cache);
		const bool usable = cached.existsAsFile() && unusableTruth (cached.loadFileAsString().toStdString()).empty();
		return truthkeeper::startPath (std::getenv ("A3_OSC_TRUTH"), cache, usable, "/usr/share/a3/a3-osc.json");
	}
}
