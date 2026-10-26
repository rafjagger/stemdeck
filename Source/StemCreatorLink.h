// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include "Session.h"
#include "StemCreator.h"
#include "StemLibrary.h"

#include <vector>

// The library and the stem creator together, the same on both screens: the
// library's folder (remembered in the settings, "stemFolder"), what is
// dropped on it asked for a target folder and queued, a new set selected,
// the creator's state shown on the library, its queue kept in the session.
class StemCreatorLink
{
public:
	// `dialogParent`: what the target-folder question is shown over.
	StemCreatorLink (StemLibrary&, StemCreator&, juce::PropertiesFile& settings, juce::Component& dialogParent);

	void showStatus();   // every tick
	void gather (std::vector<QueuedStemJob>& jobs) const;
	void restore (const std::vector<QueuedStemJob>& jobs);

private:
	void createStems (const juce::Array<juce::File>& files);
	void askTargetFolder (const juce::Array<juce::File>& files, const juce::String& preset);

	StemLibrary& library;
	StemCreator& creator;
	juce::PropertiesFile& settings;
	juce::Component& dialogParent;
	std::unique_ptr<juce::AlertWindow> stemDialog;
	std::unique_ptr<juce::FileChooser> folderChooser;
};
