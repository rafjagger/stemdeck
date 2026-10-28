#include "JackOutput.h"
#include "JackFormat.h"

#include <cstdio>

JackOutput::~JackOutput()
{
	close();
}

juce::String JackOutput::open (const juce::String& name, const juce::StringArray& portNames, juce::AudioSource& audioSource,
								const juce::StringArray& inputNames, InputSink* inputSink)
{
	close();

	jack_status_t status {};
	client = jack_client_open (name.toRawUTF8(), JackNoStartServer, &status);

	if (client == nullptr)
		return "Kein JACK-Server erreichbar";

	clientName = juce::String::fromUTF8 (jack_get_client_name (client));
	source = &audioSource;
	serverShutDown = false;
	xruns = 0;

	jack_set_process_callback (client, processCallback, this);
	jack_set_buffer_size_callback (client, bufferSizeCallback, this);
	jack_set_sample_rate_callback (client, sampleRateCallback, this);
	jack_set_xrun_callback (client, xrunCallback, this);
	jack_on_shutdown (client, shutdownCallback, this);
	jack_set_port_registration_callback (client, portRegistrationCallback, this);

	for (const auto& portName : portNames)
	{
		auto* port = jack_port_register (client, portName.toRawUTF8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

		if (port == nullptr)
		{
			close();
			return "Cannot register the JACK port: " + portName;
		}

		ports.push_back (port);
	}

	channelPointers.resize (ports.size());

	for (const auto& portName : inputNames)
	{
		auto* port = jack_port_register (client, portName.toRawUTF8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

		if (port == nullptr)
		{
			close();
			return "Cannot register the JACK port: " + portName;
		}

		inputs.push_back (port);
		inputPortNames.push_back (portName.toStdString());
	}

	inputPointers.resize (inputs.size());
	sink = inputSink;

	sampleRate = (double) jack_get_sample_rate (client);
	bufferSize = jack_get_buffer_size (client);
	renderBuffer.setSize ((int) ports.size(), (int) bufferSize.load());
	// PipeWire may not know the block size yet; the buffer-size callback prepares then.
	if (jackformat::complete (bufferSize.load(), sampleRate.load()))
		source->prepareToPlay ((int) bufferSize.load(), sampleRate.load());

	if (jack_activate (client) != 0)
	{
		close();
		return "Cannot activate the JACK client";
	}

	return {};
}

void JackOutput::close()
{
	if (client == nullptr)
		return;

	stopTimer();
	cancelPendingUpdate();
	if (linker != nullptr && ! serverShutDown.load())
		linker->removeOwned();
	linker.reset();
	reportedMissing.clear();

	if (! serverShutDown.load())
		jack_deactivate (client);

	jack_client_close (client);
	client = nullptr;
	ports.clear();
	inputs.clear();
	inputPortNames.clear();
	sink = nullptr;

	if (source != nullptr)
		source->releaseResources();

	source = nullptr;
}

bool JackOutput::isPortConnected (int index) const
{
	return isRunning() && juce::isPositiveAndBelow (index, (int) ports.size()) && jack_port_connected (ports[(size_t) index]) > 0;
}

int JackOutput::getNumConnectedPorts() const
{
	if (! isRunning())
		return 0;

	int connected = 0;

	for (auto* port : ports)
		if (jack_port_connected (port) > 0)
			++connected;

	return connected;
}

void JackOutput::keepLinks (a3::links::LinkMap map)
{
	if (! isRunning() || (map.jack.empty() && map.directLinks.empty()))
		return;

	linkMap = std::move (map);
	linker = std::make_unique<a3::links::JackLinker> (client, clientName.toStdString(), inputPortNames);
	applyLinks();
	startTimer (1000);
}

void JackOutput::applyLinks()
{
	if (linker == nullptr || ! isRunning())
		return;

	linker->apply (linkMap);
	reportMissingPeers();
}

// Once per change, not every second: a peer that is not there yet (no AES67
// stream) is a state, not news.
void JackOutput::reportMissingPeers()
{
	const auto& missing = linker->missingPeers();
	if (missing == reportedMissing)
		return;

	reportedMissing = missing;
	if (missing.empty())
	{
		std::fprintf (stderr, "%s: every link peer is there\n", clientName.toRawUTF8());
		return;
	}
	std::string clients;
	for (const auto& name : a3::links::clientNamesOf (missing))
		clients += (clients.empty() ? "" : ", ") + name;
	std::fprintf (stderr, "%s: %zu link peer ports missing (%s); linking them when they appear\n",
				  clientName.toRawUTF8(), missing.size(), clients.c_str());
}

void JackOutput::portRegistrationCallback (jack_port_id_t, int registered, void* arg)
{
	if (registered != 0)
		static_cast<JackOutput*> (arg)->triggerAsyncUpdate();
}

//==============================================================================
int JackOutput::processCallback (jack_nframes_t numFrames, void* arg)
{
	auto& self = *static_cast<JackOutput*> (arg);

	for (size_t i = 0; i < self.ports.size(); ++i)
		self.channelPointers[i] = static_cast<float*> (jack_port_get_buffer (self.ports[i], numFrames));

	// Sized on every buffer size change; growing here is a fallback that allocates.
	auto& buffer = self.renderBuffer;
	if ((int) numFrames > buffer.getNumSamples())
		buffer.setSize ((int) self.ports.size(), (int) numFrames, false, false, true);
	self.source->getNextAudioBlock (juce::AudioSourceChannelInfo (&buffer, 0, (int) numFrames));
	for (size_t i = 0; i < self.ports.size(); ++i)
		juce::FloatVectorOperations::copy (self.channelPointers[i], buffer.getReadPointer ((int) i), (int) numFrames);

	if (self.sink != nullptr && ! self.inputs.empty())
	{
		for (size_t i = 0; i < self.inputs.size(); ++i)
			self.inputPointers[i] = static_cast<const float*> (jack_port_get_buffer (self.inputs[i], numFrames));
		self.sink->inputBlock (self.inputPointers.data(), (int) self.inputPointers.size(), (int) numFrames);
	}
	return 0;
}

int JackOutput::bufferSizeCallback (jack_nframes_t numFrames, void* arg)
{
	auto& self = *static_cast<JackOutput*> (arg);

	if (numFrames != self.bufferSize.load())
	{
		self.bufferSize = numFrames;
		self.renderBuffer.setSize ((int) self.ports.size(), (int) numFrames);

		if (jackformat::complete (numFrames, self.sampleRate.load()))
			self.source->prepareToPlay ((int) numFrames, self.sampleRate.load());
	}

	return 0;
}

int JackOutput::sampleRateCallback (jack_nframes_t rate, void* arg)
{
	auto& self = *static_cast<JackOutput*> (arg);

	if (! juce::approximatelyEqual ((double) rate, self.sampleRate.load()))
	{
		self.sampleRate = (double) rate;

		if (jackformat::complete (self.bufferSize.load(), (double) rate))
			self.source->prepareToPlay ((int) self.bufferSize.load(), (double) rate);
	}

	return 0;
}

int JackOutput::xrunCallback (void* arg)
{
	++static_cast<JackOutput*> (arg)->xruns;
	return 0;
}

void JackOutput::shutdownCallback (void* arg)
{
	static_cast<JackOutput*> (arg)->serverShutDown = true;
}
