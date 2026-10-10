#pragma once

#include <atomic>
#include <string>

// The "Show the pointer" switch in Settings, as the settings file keeps it.
// StemDeck runs on a touch screen, so the pointer is hidden unless a
// developer asks for it. Pure: the look and feel asks Visibility, the
// window owns the switch.
namespace pointer
{
	constexpr const char* settingKey = "showPointer";
	constexpr bool shownByDefault = false;

	// What the settings file holds under settingKey; "" when never set.
	bool shown (const std::string& stored);
	std::string stored (bool shown);

	// Read from the message thread only today; atomic so a cursor query
	// from a peer thread can never see a torn value.
	class Visibility
	{
	public:
		bool isShown() const { return value.load(); }
		void set (bool shown) { value.store (shown); }

	private:
		std::atomic<bool> value { shownByDefault };
	};
}
