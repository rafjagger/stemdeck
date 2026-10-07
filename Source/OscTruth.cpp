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

	Endpoints endpointsFrom (const std::vector<Listener>& listeners, const std::map<std::string, std::string>& hosts)
	{
		const auto hostOf = [&hosts] (const std::string& name) {
			const auto found = hosts.find (name);
			return found != hosts.end() ? found->second : std::string();
		};
		Endpoints e;
		for (const auto& listener : listeners)
		{
			if (listener.role != "osc")
				continue;
			if (listener.program == "core")
			{
				e.coreHost = hostOf ("core");
				e.corePort = listener.port;
			}
			else if (listener.program == "mixer")
			{
				e.mixerHost = hostOf (listener.host);
				e.mixerPort = listener.port;
			}
			else if (listener.program == "stemdeck")
				e.ownPort = listener.port;
		}
		return e;
	}

	std::string truthPath()
	{
		const auto* overridden = std::getenv ("A3_OSC_TRUTH");
		return overridden != nullptr && *overridden != '\0' ? overridden : "/usr/share/a3/a3-osc.json";
	}

	MeterParameters meterParametersFrom (const std::map<std::string, double>& meters)
	{
		MeterParameters parameters;
		const auto take = [&meters] (const char* key, float& value, bool zeroAllowed)
		{
			const auto found = meters.find (key);
			if (found == meters.end())
				return;
			if (found->second > 0.0 || (zeroAllowed && found->second == 0.0))
				value = (float) found->second;
		};
		take ("attack_ms", parameters.attackMs, true);
		take ("release_db_per_second", parameters.releaseDbPerSecond, false);
		take ("peak_hold_seconds", parameters.peakHoldSeconds, true);
		return parameters;
	}
}
