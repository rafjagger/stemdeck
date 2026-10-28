#include "MasterDeck.h"

#include <algorithm>
#include <cstddef>

int chooseMaster (int pressed, std::array<bool, 2> playing, int current, bool automatic)
{
	if (pressed == 0 || pressed == 1)
		return pressed == current ? -1 : pressed;  // pressed on the master: off

	if (current == 0 || current == 1)
		return ! playing[static_cast<std::size_t> (current)] && playing[static_cast<std::size_t> (1 - current)] ? 1 - current : current;

	if (automatic && playing[0] != playing[1])
		return playing[0] ? 0 : 1;

	return -1;
}

void InstanceMaster::joined (double seconds)
{
	joinedAt = seconds;
	claims.clear();
}

void InstanceMaster::press (double seconds)
{
	master = ! master;
	claimedAt = seconds;
}

void InstanceMaster::take (double seconds)
{
	if (master)
		return;
	master = true;
	claimedAt = seconds;
}

void InstanceMaster::onStatus (int device, bool claimsMaster, double seconds)
{
	if (! claimsMaster)
	{
		claims.erase (device);
		return;
	}

	auto found = claims.find (device);
	if (found != claims.end() && seconds - found->second.last <= silence)
	{
		found->second.last = seconds;
		return;
	}

	// A new claim -- unless heard just after joining: then it was running
	// already, older than any press here, even one made before the join (the
	// link may start on a retry, seconds after MASTER was pressed).
	const auto since = seconds - joinedAt <= joinGrace ? std::min (joinedAt - joinGrace, claimedAt) : seconds;
	claims[device] = { since, seconds };
}

bool InstanceMaster::yieldIfOvertaken (double seconds)
{
	if (! master)
		return false;

	for (const auto& [device, claim] : claims)
		if (seconds - claim.last <= silence && claim.since > claimedAt)
		{
			master = false;
			return true;
		}
	return false;
}
