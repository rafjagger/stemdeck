// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioOutput.h"

#include "AudioPanel.h"
#include "Theme.h"

AudioOutput::AudioOutput (juce::PropertiesFile& settingsFile) : settings (settingsFile) {}

AudioOutput::~AudioOutput()
{
	close();
}

void AudioOutput::open (const juce::String& clientName, const juce::StringArray& portNames, juce::AudioSource& source,
                        const juce::StringArray& inputNames, JackOutput::InputSink* inputSink)
{
	jackOpen = jack.open (clientName, portNames, source, inputNames, inputSink).isEmpty();
	if (! jackOpen)
		openDevice (portNames.size(), source);
}

void AudioOutput::openDevice (int numOutputs, juce::AudioSource& source)
{
	deviceOutputs = numOutputs;
	const auto savedState = settings.getXmlValue ("audioDeviceState");

	// Also the way back in when the output mode changed: nothing doubled up.
	deviceManager.removeChangeListener (this);
	deviceManager.removeAudioCallback (&audioSourcePlayer);
	deviceManager.closeAudioDevice();

	deviceManager.initialise (0, numOutputs, savedState.get(), true);

	// First start: enable as many of the outputs as the device has.
	if (savedState == nullptr)
	{
		auto setup = deviceManager.getAudioDeviceSetup();
		setup.useDefaultOutputChannels = false;
		setup.outputChannels.clear();
		setup.outputChannels.setRange (0, numOutputs, true);
		deviceManager.setAudioDeviceSetup (setup, true);
	}

	deviceManager.addChangeListener (this);
	audioSourcePlayer.setSource (&source);
	deviceManager.addAudioCallback (&audioSourcePlayer);
}

void AudioOutput::closeJack()
{
	jack.close();
}

void AudioOutput::close()
{
	jack.close();
	deviceManager.removeChangeListener (this);
	deviceManager.removeAudioCallback (&audioSourcePlayer);
	audioSourcePlayer.setSource (nullptr);
	deviceManager.closeAudioDevice();
}

void AudioOutput::changeListenerCallback (juce::ChangeBroadcaster*)
{
	if (auto state = deviceManager.createStateXml())
		settings.setValue ("audioDeviceState", state.get());

	if (onDeviceChanged)
		onDeviceChanged();
}

int AudioOutput::activeDeviceOutputs() const
{
	const auto* device = jackOpen ? nullptr : deviceManager.getCurrentAudioDevice();
	return device == nullptr ? 0 : device->getActiveOutputChannels().countNumberOfSetBits();
}

AudioOutput::Status AudioOutput::status (int wantedDeviceOutputs) const
{
	if (jackOpen)
	{
		if (! jack.isRunning())
			return { "The JACK server has stopped - please restart StemDeck", Health::failed };

		juce::String text;
		text << "JACK: " << jack.getClientName() << "  |  " << juce::String (jack.getSampleRate(), 0) << " Hz, "
			 << jack.getBufferSize() << " Samples  |  " << jack.getNumPorts() << " Ports, "
			 << jack.getNumConnectedPorts() << " connected  |  Xruns: " << jack.getXrunCount();
		return { text, Health::fine };
	}

	auto* device = deviceManager.getCurrentAudioDevice();
	if (device == nullptr)
		return { "No audio device open", Health::failed };

	const auto activeOutputs = device->getActiveOutputChannels().countNumberOfSetBits();
	auto text = deviceManager.getCurrentAudioDeviceType() + ": " + device->getName()
			  + "  |  " + juce::String (device->getCurrentSampleRate(), 0) + " Hz, "
			  + juce::String (device->getCurrentBufferSizeSamples()) + " Samples  |  "
			  + juce::String (activeOutputs) + juce::String (" outputs");

	if (activeOutputs < wantedDeviceOutputs)
		text << "  (summed down)";
	return { text, activeOutputs < wantedDeviceOutputs ? Health::warning : Health::fine };
}

void AudioOutput::showDeviceSettings()
{
	juce::DialogWindow::LaunchOptions dialog;
	dialog.content.setOwned (new AudioPanel (deviceManager, deviceOutputs));
	dialog.dialogTitle = "Audio";
	dialog.dialogBackgroundColour = Theme::panel;
	dialog.useNativeTitleBar = true;
	dialog.resizable = true;
	dialog.launchAsync();
}
