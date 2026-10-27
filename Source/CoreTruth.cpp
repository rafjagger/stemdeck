// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "CoreTruth.h"

#include "OscTruthFile.h"

#include <cstdlib>
#include <iostream>

namespace coretruth
{
	Chosen choose()
	{
		Chosen truth;
		truth.path = osctruth::liveTruthPath();
		truth.hash = juce::SHA256 (juce::File (truth.path)).toHexString();
		return truth;
	}

	std::unique_ptr<TruthKeeperLink> follow (const Chosen& truth)
	{
		if (! truthkeeper::followsCore (std::getenv ("A3_OSC_TRUTH")))
		{
			std::cerr << "StemDeck: A3_OSC_TRUTH is set: not following Core's truth" << std::endl;
			return nullptr;
		}

		const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName().toStdString();
		auto keeper = std::make_unique<TruthKeeperLink> (
			truth.hash.toStdString(),
			juce::File (truthkeeper::cachePath (home)),
			[] (const juce::String& body) { return juce::String (osctruth::unusableTruth (body.toStdString())); },
			[] {
				juce::JUCEApplication::getInstance()->setApplicationReturnValue (1);
				juce::JUCEApplication::quit();
			});
		// radla's subnet never hears the broadcast: there a file names Core.
		const auto core = juce::File (truthkeeper::corePath (home));
		const auto url = core.existsAsFile() ? truthkeeper::pollUrl (core.loadFileAsString().toStdString()) : std::string();
		if (url.empty())
		{
			if (core.existsAsFile() && core.loadFileAsString().trim().isNotEmpty())
				std::cerr << "StemDeck: " << core.getFullPathName() << " does not name an http:// address;"
						  << " listening for Core's broadcast instead" << std::endl;
			keeper->start();
		}
		else
		{
			std::cerr << "StemDeck: following Core's truth at " << url << " every "
					  << truthkeeper::pollSeconds << " s" << std::endl;
			keeper->startPolling (juce::String (url));
		}
		return keeper;
	}
}
