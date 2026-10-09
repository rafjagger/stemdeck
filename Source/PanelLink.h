#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// Finding the A³ Motion panel and keeping hold of it. Pure: no JUCE; the
// sysfs walk takes its root as a parameter so a test can hand it a fake tree.
namespace panel
{
	// The panel's USB-serial bridge, a CH343 (a3-motion's board file,
	// firmware/boards/esp32-s3-devkitc-1-n16r8.json). A tty number is not an
	// identity -- ttyACM0 on one machine is something else on the next -- so
	// this is the only thing a port is chosen by.
	constexpr const char* usbVendor = "1a86";
	constexpr const char* usbProduct = "55d3";

	// The device paths of every tty under `classTty` (/sys/class/tty) whose
	// USB device carries the panel's ID, in natural name order (ttyACM2
	// before ttyACM10). Nothing else is ever a candidate.
	std::vector<std::string> findPorts (const std::filesystem::path& classTty);

	// When to look for a panel that is not open, and when to give up on one
	// that has gone quiet.
	class Reconnect
	{
	public:
		// Two seconds of silence at the quarter-second read timeout: a lost
		// frame passes, a pulled cable does not.
		static constexpr int quietPollsBeforeClosing = 8;
		// No panel: looked for at once, then every two seconds -- not on every
		// turn of the poll loop.
		static constexpr std::int64_t lookEveryMs = 2000;

		bool shouldLook (std::int64_t nowMs);
		// A poll came back with nothing. True when the port should be let go;
		// the count starts over.
		bool noteQuietPoll();
		void noteFrame() { quietPolls = 0; }

	private:
		int quietPolls = 0;
		std::int64_t lastLookMs = 0;
		bool lookedYet = false;
	};
}
