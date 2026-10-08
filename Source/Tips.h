#pragma once

#include <string>

// The tips (tooltips) switch in Settings (2026-10-08), as the settings file
// keeps it. Pure: the window owns the TooltipWindow and makes or drops it.
namespace tips
{
	constexpr const char* settingKey = "showTips";
	constexpr bool shownByDefault = true;

	// What the settings file holds under settingKey; "" when never set.
	bool shown (const std::string& stored);
	std::string stored (bool shown);
}
