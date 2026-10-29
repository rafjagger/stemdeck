#include "SyncLabels.h"

#include <cstdio>

namespace
{
	const std::string network = "LINK";
	const std::string dot = "\xc2\xb7"; // U+00B7 middle dot
	const std::string dash = "\xe2\x80\x93"; // U+2013 en dash

	std::string oneDecimal (double value)
	{
		char text[32];
		std::snprintf (text, sizeof (text), "%.1f", value);
		return text;
	}
}

namespace syncLabels
{
	std::string sourceButton (bool fromNetwork) { return fromNetwork ? "SYNC: " + network : "SYNC: DECK"; }

	std::string player (int number) { return "Player " + std::to_string (number); }

	std::string following (double bpm, int leader)
	{
		return network + " " + oneDecimal (bpm) + " " + dot + " " + (leader > 0 ? player (leader) : "held");
	}

	std::string nothingHeard() { return network + " " + dash; }

	std::string noMaster() { return network + ": no master " + dot + " follow"; }

	std::string error (const std::string& why) { return network + ": " + why; }

	std::string sendingMaster (int deck) { return network + " master: " + std::string (1, (char) ('A' + deck)); }

	std::string masterTooltip() { return "This deck gives the beat to the DJ network (its tempo master)"; }

	std::string linkSyncTooltip() { return "Lock tempo and beats to the master on the DJ network: another StemDeck deck, or a player"; }

	std::string linkUnavailable() { return "SYNC and MASTER need the DJ network's ports in a3-osc.json"; }

	std::string gridTooltip() { return "Grid Adjust: the controller's jog wheel moves the beat grid"; }
}
