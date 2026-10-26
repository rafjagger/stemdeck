// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "AppSettings.h"

namespace appsettings
{
	juce::PropertiesFile::Options options()
	{
		juce::PropertiesFile::Options options;
		options.applicationName = "StemDeck";
	   #if JUCE_LINUX
		options.folderName = "~/.config/StemDeck";
	   #else
		options.folderName = "StemDeck";
	   #endif
		options.filenameSuffix = ".settings";
		options.osxLibrarySubFolder = "Application Support";
		return options;
	}

	juce::File analysisCacheFile()
	{
		return options().getDefaultFile().getSiblingFile ("analysis.xml");
	}
}
