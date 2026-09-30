#include "OscTruth.h"

#include <cstdlib>

namespace osctruth
{
	namespace
	{
		int portOf (const std::vector<Listener>& listeners, const std::string& role)
		{
			for (const auto& listener : listeners)
				if (listener.program == "prolink" && listener.role == role)
					return listener.port;
			return -1;
		}
	}

	ProLinkPorts proLinkPortsFrom (const std::vector<Listener>& listeners)
	{
		return { portOf (listeners, "announce"), portOf (listeners, "beat"), portOf (listeners, "status") };
	}

	std::string truthPath()
	{
		const auto* overridden = std::getenv ("A3_OSC_TRUTH");
		return overridden != nullptr && *overridden != '\0' ? overridden : "/usr/share/a3/a3-osc.json";
	}
}
