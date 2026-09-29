#include "Scs3dDevice.h"

Scs3dDevice::Scs3dDevice (const juce::MidiDeviceInfo& input, const juce::MidiDeviceInfo& output)
	: inputInfo (input)
{
	out = juce::MidiOutput::openDevice (output.identifier);
	in = juce::MidiInput::openDevice (input.identifier, this);
	if (! isOpen())
		return;

	for (const auto& sysex : scs3d::initSysex())
		out->sendMessageNow (juce::MidiMessage::createSysExMessage (sysex.data() + 1, (int) sysex.size() - 2));
	in->start();
}

Scs3dDevice::~Scs3dDevice()
{
	*alive = false;
	if (in != nullptr)
		in->stop();
	if (out != nullptr)
	{
		// Dark and back to its plain mode: nothing left lit for a device StemDeck let go of.
		for (const auto& [id, value] : sent)
			if (value != 0)
				out->sendMessageNow (juce::MidiMessage::noteOn (1, id, (juce::uint8) 0));
		const auto flat = scs3d::flatModeSysex();
		out->sendMessageNow (juce::MidiMessage::createSysExMessage (flat.data() + 1, (int) flat.size() - 2));
	}
}

std::vector<std::pair<juce::MidiDeviceInfo, juce::MidiDeviceInfo>> Scs3dDevice::find()
{
	const auto matching = [] (juce::Array<juce::MidiDeviceInfo> devices)
	{
		std::vector<juce::MidiDeviceInfo> found;
		for (const auto& d : devices)
			if (d.name.containsIgnoreCase ("SCS.3d"))
				found.push_back (d);
		std::sort (found.begin(), found.end(), [] (const auto& a, const auto& b) { return a.identifier.compareNatural (b.identifier) < 0; });
		return found;
	};

	const auto inputs = matching (juce::MidiInput::getAvailableDevices());
	const auto outputs = matching (juce::MidiOutput::getAvailableDevices());

	std::vector<std::pair<juce::MidiDeviceInfo, juce::MidiDeviceInfo>> pairs;
	for (size_t i = 0; i < std::min (inputs.size(), outputs.size()); ++i)
		pairs.emplace_back (inputs[i], outputs[i]);
	return pairs;
}

void Scs3dDevice::show (const scs3d::Leds& leds)
{
	if (out == nullptr)
		return;

	for (const auto& m : scs3d::render (leds))
	{
		const auto it = sent.find (m[1]);
		if (it != sent.end() && it->second == m[2])
			continue;
		sent[m[1]] = m[2];
		out->sendMessageNow (juce::MidiMessage (m[0], m[1], m[2]));
	}
}

void Scs3dDevice::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
	if (message.getRawDataSize() < 3 || message.isSysEx())
		return;

	const auto* raw = message.getRawData();
	const auto event = scs3d::decode (raw[0], raw[1], raw[2]);
	if (event.type == scs3d::Event::Type::none)
		return;

	juce::MessageManager::callAsync ([this, event, alive = alive]
	{
		if (*alive && onEvent)
			onEvent (event);
	});
}
