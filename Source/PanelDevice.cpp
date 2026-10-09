#include "PanelDevice.h"

#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace
{
	constexpr int readTimeoutMs = 250;
	// The pots move slowly next to the keys: asked for every fifth poll.
	constexpr int potEvery = 5;
	constexpr int pollGapMs = 5;
	// Opening the port can reset the board through its auto-reset circuit;
	// the first questions land in its boot. About three seconds all told,
	// more than an ESP32-S3 needs to its first line.
	constexpr int bootWaitMs = 600;
	constexpr int pingAttempts = 6;
	constexpr int pingRetryMs = 400;
}

PanelDevice::PanelDevice() : juce::Thread ("A3 Motion panel")
{
	startThread();
}

PanelDevice::~PanelDevice()
{
	*alive = false;
	stopThread (4000);
}

void PanelDevice::show (const panel::LedState& state)
{
	const auto leds = panel::render (state);
	const std::lock_guard<std::mutex> lock (ledLock);
	wanted = leds;
}

void PanelDevice::run()
{
	while (! threadShouldExit())
	{
		if (fd < 0)
		{
			if (reconnect.shouldLook (juce::Time::currentTimeMillis()))
				for (const auto& path : panel::findPorts ("/sys/class/tty"))
					if (open (path))
						break;
			if (fd < 0)
			{
				wait (100);
				continue;
			}
		}

		writeLeds();
		if (poll())
			wait (pollGapMs);
	}

	// Left dark: nothing lit on a panel StemDeck let go of.
	if (fd >= 0)
	{
		const std::lock_guard<std::mutex> lock (ledLock);
		wanted = {};
	}
	writeLeds();
	close();
}

bool PanelDevice::open (const std::string& path)
{
	fd = ::open (path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0)
	{
		// EBUSY: A³ Motion holds it exclusively.
		return false;
	}

	// Ours alone, or not at all: a second program writing LEDs and reading
	// frames on the same port garbles both.
	if (::flock (fd, LOCK_EX | LOCK_NB) != 0 || ::ioctl (fd, TIOCEXCL) != 0)
	{
		note ("Panel: " + juce::String (path) + " is in use by another program");
		::close (fd);
		fd = -1;
		return false;
	}

	termios tty {};
	::tcgetattr (fd, &tty);
	::cfmakeraw (&tty);
	::cfsetispeed (&tty, B115200);
	::cfsetospeed (&tty, B115200);
	tty.c_cflag |= CLOCAL | CREAD;
	tty.c_cflag &= ~(tcflag_t) CRTSCTS;
	::tcsetattr (fd, TCSANOW, &tty);

	// DTR and RTS run to the ESP32's auto-reset circuit through the CH343:
	// asserted they hold it in reset or bring it up in its download stub.
	// Opening asserts them; dropped is "run normally".
	int lines = TIOCM_DTR | TIOCM_RTS;
	::ioctl (fd, TIOCMBIC, &lines);

	wait (bootWaitMs);
	if (threadShouldExit() || ! pingAnswers())
	{
		note ("Panel: " + juce::String (path) + " did not answer PING");
		close();
		return false;
	}

	openPath = path;
	decoder.reset();
	reconnect.noteFrame();
	shownKnown = false;   // whatever it shows, it was not this program's doing
	cycle = 0;
	connected = true;
	note ("Panel: A3 Motion panel on " + juce::String (path));
	return true;
}

bool PanelDevice::pingAnswers()
{
	for (int attempt = 0; attempt < pingAttempts && ! threadShouldExit(); ++attempt)
	{
		// The boot's chatter is not an answer to a question nobody asked yet.
		::tcflush (fd, TCIFLUSH);
		std::uint8_t reply = 0;
		if (writeAll (&panel::ping, 1) && readExact (&reply, 1, readTimeoutMs) && reply == panel::ping)
		{
			::tcflush (fd, TCIFLUSH);
			return true;
		}
		wait (pingRetryMs);
	}
	return false;
}

void PanelDevice::close()
{
	if (fd < 0)
		return;
	::close (fd);   // the flock and TIOCEXCL go with it
	fd = -1;
	if (connected.exchange (false))
		note ("Panel: let go of " + juce::String (openPath));
}

bool PanelDevice::poll()
{
	const auto withPots = cycle++ % potEvery == 0;
	const auto request = panel::pollRequest (withPots);
	std::uint8_t reply[64];
	const auto size = panel::pollReplySize (withPots);

	if (! writeAll (request.data(), request.size()))
		return false;

	if (! readExact (reply, size, readTimeoutMs))
	{
		if (fd >= 0 && reconnect.noteQuietPoll())
			close();
		return fd >= 0;
	}

	const auto frame = panel::parsePollReply (reply, size, withPots);
	if (! frame)
	{
		// A stream that slipped repeats its slip on every frame: start clean.
		::tcflush (fd, TCIFLUSH);
		return true;
	}

	reconnect.noteFrame();
	if (auto events = decoder.feed (*frame); ! events.empty())
		post (std::move (events));
	return true;
}

bool PanelDevice::writeAll (const std::uint8_t* data, size_t size)
{
	while (size > 0 && fd >= 0)
	{
		const auto written = ::write (fd, data, size);
		if (written > 0)
		{
			data += written;
			size -= (size_t) written;
			continue;
		}
		if (written < 0 && (errno == EAGAIN || errno == EINTR))
		{
			pollfd p { fd, POLLOUT, 0 };
			if (::poll (&p, 1, readTimeoutMs) > 0 && (p.revents & (POLLERR | POLLHUP)) == 0)
				continue;
		}
		// Unplugged: the tty is gone (EIO), not merely slow.
		close();
		return false;
	}
	return fd >= 0;
}

bool PanelDevice::readExact (std::uint8_t* data, size_t size, int timeoutMs)
{
	const auto deadline = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
	while (size > 0 && fd >= 0)
	{
		const auto left = (int) (deadline - juce::Time::getMillisecondCounter());
		if (left <= 0 || left > timeoutMs)
			return false;

		pollfd p { fd, POLLIN, 0 };
		const auto ready = ::poll (&p, 1, left);
		if (ready == 0)
			return false;
		if (ready < 0 && errno == EINTR)
			continue;
		if (ready < 0 || (p.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
		{
			close();
			return false;
		}

		const auto got = ::read (fd, data, size);
		if (got > 0)
		{
			data += got;
			size -= (size_t) got;
		}
		else if (got == 0 || (errno != EAGAIN && errno != EINTR))
		{
			close();
			return false;
		}
	}
	return size == 0;
}

void PanelDevice::writeLeds()
{
	panel::Leds next;
	{
		const std::lock_guard<std::mutex> lock (ledLock);
		next = wanted;
	}

	// One write for every LED that changed: the link is shared with the polls.
	std::vector<std::uint8_t> out;
	for (int led = 0; led < panel::ledCount; ++led)
	{
		const auto colour = next[(size_t) led];
		if (shownKnown && colour == shown[(size_t) led])
			continue;
		const auto frame = panel::setLed (led, colour);
		out.insert (out.end(), frame.begin(), frame.end());
	}
	if (out.empty() || ! writeAll (out.data(), out.size()))
		return;

	shown = next;
	shownKnown = true;
}

void PanelDevice::note (const juce::String& line)
{
	// Looked for every two seconds: the same refusal once, not every time.
	if (line == lastNote)
		return;
	lastNote = line;
	juce::Logger::writeToLog (line);
}

void PanelDevice::post (std::vector<panel::Event> events)
{
	juce::MessageManager::callAsync ([this, events = std::move (events), alive = alive]
	{
		if (! *alive || ! onEvent)
			return;
		for (const auto& e : events)
			onEvent (e);
	});
}
