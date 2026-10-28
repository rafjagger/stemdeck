#include "ProLinkReceiver.h"

#include "LinkInterface.h"

#include <JuceHeader.h>

#include <arpa/inet.h>
#include <utility>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
	// Wakes for the announcer too: a CDJ's keep-alive gap is held to a few ms.
	constexpr int pollMs = 10;

	double now() { return juce::Time::getMillisecondCounterHiRes() / 1000.0; }
}

ProLinkReceiver::~ProLinkReceiver()
{
	stop();
}

std::string ProLinkReceiver::error() const
{
	std::lock_guard<std::mutex> guard (lock);
	return lastError;
}

bool ProLinkReceiver::openSocket (int& fd, int port)
{
	fd = socket (AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		return false;

	const int on = 1;
	setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on));
	setsockopt (fd, SOL_SOCKET, SO_BROADCAST, &on, sizeof (on));

	sockaddr_in address {};
	address.sin_family = AF_INET;
	address.sin_port = htons ((uint16_t) port);
	address.sin_addr.s_addr = htonl (INADDR_ANY);
	return bind (fd, reinterpret_cast<sockaddr*> (&address), sizeof (address)) == 0;
}

bool ProLinkReceiver::start (int number, const osctruth::ProLinkPorts& truthPorts,
							 const std::string& deviceName)
{
	stop();
	deviceNumber = number;
	ports = truthPorts;
	name = deviceName;

	const auto fail = [this] (const std::string& why)
	{
		for (auto* fd : { &sockAnnounce, &sockBeat, &sockStatus })
			if (*fd >= 0) { close (*fd); *fd = -1; }
		std::lock_guard<std::mutex> guard (lock);
		lastError = why;
		return false;
	};

	if (! ports.complete())
		return fail ("no a3-osc.json");

	prolink::Identity self { number, deviceName, forcedMac, forcedLocalIp };
	broadcastIp = forcedBroadcastIp;
	if (forcedBroadcastIp == 0)
	{
		std::string why;
		const auto found = linkinterface::find (why);
		if (! found)
			return fail (why);
		self.ip = found->local;
		self.mac = found->mac;
		broadcastIp = found->broadcast;
	}
	announcer = std::make_unique<prolink::Announcer> (self);
	tempo = std::make_unique<prolink::TempoMaster> (number, deviceName, now());
	addresses.clear();
	ownIp = self.ip;
	lastWantsMaster = false;
	currentDevice = number;
	online = false;

	if (! openSocket (sockAnnounce, ports.announce)
		|| ! openSocket (sockBeat, ports.beat)
		|| ! openSocket (sockStatus, ports.status))
		return fail ("ports busy");

	{
		std::lock_guard<std::mutex> guard (lock);
		lastError.clear();
		events.clear();
		role = {};
		pendingChange = prolink::TempoMaster::Change::none;
	}
	running = true;
	thread = std::thread ([this] { run(); });
	return true;
}

void ProLinkReceiver::stop()
{
	if (! running.exchange (false))
		return;

	for (auto* fd : { &sockAnnounce, &sockBeat, &sockStatus })
		if (*fd >= 0) { ::shutdown (*fd, SHUT_RDWR); close (*fd); *fd = -1; }

	if (thread.joinable())
		thread.join();
	online = false;
}

std::vector<ProLinkReceiver::Event> ProLinkReceiver::drain()
{
	std::lock_guard<std::mutex> guard (lock);
	std::vector<Event> out;
	out.swap (events);
	return out;
}

void ProLinkReceiver::announce (const std::vector<prolink::Announcer::Send>& packets)
{
	for (const auto& packet : packets)
	{
		sockaddr_in to {};
		to.sin_family = AF_INET;
		to.sin_port = htons ((uint16_t) ports.announce);
		to.sin_addr.s_addr = packet.to != 0 ? packet.to : broadcastIp;
		sendto (sockAnnounce, packet.bytes.data(), packet.bytes.size(), 0, reinterpret_cast<sockaddr*> (&to), sizeof (to));
	}
}

void ProLinkReceiver::hearAnnouncement (double at)
{
	uint8_t buffer[600];
	sockaddr_in from {};
	socklen_t fromLength = sizeof (from);
	const auto n = recvfrom (sockAnnounce, buffer, sizeof (buffer), 0, reinterpret_cast<sockaddr*> (&from), &fromLength);
	if (n > 0)
		announce (announcer->hear (buffer, (size_t) n, from.sin_addr.s_addr, at));
}

prolink::MasterState ProLinkReceiver::masterState() const
{
	std::lock_guard<std::mutex> guard (lock);
	return role;
}

void ProLinkReceiver::reportLocal (bool hasTrack, bool playing, bool synced)
{
	localTrack = hasTrack;
	localPlaying = playing;
	localSynced = synced;
}

prolink::TempoMaster::Change ProLinkReceiver::takeMasterChange()
{
	std::lock_guard<std::mutex> guard (lock);
	return std::exchange (pendingChange, prolink::TempoMaster::Change::none);
}

// Port 50001, unicast to the device as a CDJ does -- except to a device on
// this host, which shares the port with us and beat-analyzer: a unicast would
// reach only the socket bound last, so it goes out as broadcast (#70).
void ProLinkReceiver::sendControl (const std::vector<prolink::TempoMaster::Send>& packets)
{
	for (const auto& packet : packets)
	{
		const auto found = addresses.find (packet.to);
		if (found == addresses.end())
			continue;
		sockaddr_in to {};
		to.sin_family = AF_INET;
		to.sin_port = htons ((uint16_t) ports.beat);
		to.sin_addr.s_addr = found->second == ownIp ? broadcastIp : found->second;
		sendto (sockBeat, packet.bytes.data(), packet.bytes.size(), 0, reinterpret_cast<sockaddr*> (&to), sizeof (to));
	}
}

// The role follows the number (a mixer may assign one), the deck the sender
// reports and the DJ's MASTER; it acts only once the number is ours.
void ProLinkReceiver::runTempoMaster (double at)
{
	tempo->setDevice (announcer->device());
	tempo->setLocal (localTrack.load(), localPlaying.load(), localSynced.load());
	if (const auto wanted = wantsMaster.load(); wanted != lastWantsMaster)
	{
		tempo->want (wanted, at);
		lastWantsMaster = wanted;
	}
	if (online)
		sendControl (tempo->tick (at));

	std::lock_guard<std::mutex> guard (lock);
	role = tempo->state();
	if (const auto change = tempo->takeChange(); change != prolink::TempoMaster::Change::none)
		pendingChange = change;
}

void ProLinkReceiver::hearStatus (double at)
{
	uint8_t buffer[600]; // CDJ-3000 status packets reach 0x200 bytes
	sockaddr_in from {};
	socklen_t fromLength = sizeof (from);
	const auto n = recvfrom (sockStatus, buffer, sizeof (buffer), 0, reinterpret_cast<sockaddr*> (&from), &fromLength);
	if (n <= 0)
		return;

	const auto own = currentDevice.load();
	if (auto status = prolink::parseStatus (buffer, (size_t) n); status && status->device != 0 && status->device != own)
	{
		addresses[status->device] = from.sin_addr.s_addr;
		sendControl (tempo->hear (*status, at));
		std::lock_guard<std::mutex> guard (lock);
		events.push_back ({ *status, at });
	}
	else if (auto mixer = prolink::parseMixerStatus (buffer, (size_t) n); mixer && mixer->device != own)
	{
		addresses[mixer->device] = from.sin_addr.s_addr;
		sendControl (tempo->hear (*mixer, at));
	}
}

// Port 50001: beats, and the takeover and sync control packets.
void ProLinkReceiver::hearBeatPort (double at)
{
	uint8_t buffer[600];
	sockaddr_in from {};
	socklen_t fromLength = sizeof (from);
	const auto n = recvfrom (sockBeat, buffer, sizeof (buffer), 0, reinterpret_cast<sockaddr*> (&from), &fromLength);
	if (n <= 0)
		return;

	const auto own = currentDevice.load();
	if (auto beat = prolink::parseBeat (buffer, (size_t) n); beat && beat->device != own)
	{
		std::lock_guard<std::mutex> guard (lock);
		events.push_back ({ *beat, at });
	}
	else if (auto control = prolink::parseControl (buffer, (size_t) n); control && control->device != own)
	{
		addresses[control->device] = from.sin_addr.s_addr;
		sendControl (tempo->hear (*control, at));
	}
}

void ProLinkReceiver::run()
{
	while (running)
	{
		announce (announcer->due (now()));
		online = announcer->isOnline();
		currentDevice = announcer->device();
		runTempoMaster (now());
		if (! announcer->refusal().empty())
		{
			std::lock_guard<std::mutex> guard (lock);
			lastError = announcer->refusal();
		}

		pollfd fds[] = { { sockAnnounce, POLLIN, 0 }, { sockBeat, POLLIN, 0 }, { sockStatus, POLLIN, 0 } };
		if (poll (fds, 3, pollMs) <= 0)
			continue;

		if (fds[0].revents & POLLIN)
			hearAnnouncement (now());

		if (fds[1].revents & POLLIN)
			hearBeatPort (now());

		if (fds[2].revents & POLLIN)
			hearStatus (now());
	}
}

void ProLinkReceiver::useInterface (uint32_t localIpNetworkOrder, uint32_t broadcastIpNetworkOrder,
									const std::array<uint8_t, 6>& macAddress)
{
	forcedMac = macAddress;
	forcedLocalIp = localIpNetworkOrder;
	forcedBroadcastIp = broadcastIpNetworkOrder;
}
