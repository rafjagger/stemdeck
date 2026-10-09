#include "PanelLink.h"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace fs = std::filesystem;

namespace panel
{
	namespace
	{
		std::string firstLine (const fs::path& file)
		{
			std::ifstream in (file);
			std::string line;
			std::getline (in, line);
			std::transform (line.begin(), line.end(), line.begin(), [] (unsigned char c) { return (char) std::tolower (c); });
			return line;
		}

		// An ACM tty's device is the USB interface, a usb-serial one's sits a
		// level below it; either way the USB device is the first parent that
		// carries idVendor and idProduct.
		bool isPanel (const fs::path& classTty, const std::string& name)
		{
			std::error_code ec;
			auto dir = fs::canonical (classTty / name / "device", ec);
			if (ec)
				return false;

			for (int depth = 0; depth < 32 && ! dir.empty(); ++depth)
			{
				if (fs::is_regular_file (dir / "idVendor", ec) && fs::is_regular_file (dir / "idProduct", ec))
					return firstLine (dir / "idVendor") == usbVendor && firstLine (dir / "idProduct") == usbProduct;
				if (dir == dir.parent_path())
					break;
				dir = dir.parent_path();
			}
			return false;
		}

		// ttyACM2 before ttyACM10: the digits compared as a number.
		bool naturalLess (const std::string& a, const std::string& b)
		{
			const auto split = [] (const std::string& s)
			{
				const auto digits = s.find_first_of ("0123456789");
				const auto prefix = s.substr (0, digits);
				const auto number = digits == std::string::npos ? -1L : std::strtol (s.c_str() + digits, nullptr, 10);
				return std::make_pair (prefix, number);
			};
			return split (a) < split (b);
		}
	}

	std::vector<std::string> findPorts (const fs::path& classTty)
	{
		std::vector<std::string> names;
		std::error_code ec;
		for (fs::directory_iterator it (classTty, ec), end; ! ec && it != end; it.increment (ec))
		{
			const auto name = it->path().filename().string();
			if (isPanel (classTty, name))
				names.push_back (name);
		}

		std::sort (names.begin(), names.end(), naturalLess);
		std::vector<std::string> ports;
		for (const auto& name : names)
			ports.push_back ("/dev/" + name);
		return ports;
	}

	bool Reconnect::shouldLook (std::int64_t nowMs)
	{
		if (lookedYet && nowMs - lastLookMs < lookEveryMs)
			return false;
		lookedYet = true;
		lastLookMs = nowMs;
		quietPolls = 0;
		return true;
	}

	bool Reconnect::noteQuietPoll()
	{
		if (++quietPolls < quietPollsBeforeClosing)
			return false;
		quietPolls = 0;
		return true;
	}
}
