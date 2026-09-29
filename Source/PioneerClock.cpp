#include "PioneerClock.h"

void PioneerClock::onStatus (const prolink::StatusPacket& status, double seconds)
{
	lastStatus = seconds;

	if (status.isMaster && status.isPlaying)
		master = status.device;
	else if (status.device == master)
		master = 0;   // gave the master away, or stopped
}

void PioneerClock::onBeat (const prolink::BeatPacket& beat, double seconds)
{
	// Nobody to follow yet: the first player heard is the choice, and stays it.
	if (chosen == 0 && ! hasMasterInfo (seconds))
		chosen = beat.device;

	if (beat.device != leader (seconds))
		return;

	lastBeat = seconds;
	tempo = beat.effectiveBpm;
}

void PioneerClock::choosePlayer (int device)
{
	chosen = device;
}

int PioneerClock::leader (double seconds) const
{
	if (hasMasterInfo (seconds) && master != 0)
		return master;
	return chosen;
}

bool PioneerClock::hasMasterInfo (double seconds) const
{
	return lastStatus >= 0.0 && seconds - lastStatus <= silence;
}

bool PioneerClock::isLive (double seconds) const
{
	return lastBeat >= 0.0 && seconds - lastBeat <= silence;
}

double PioneerClock::beatPhaseAt (double seconds) const
{
	if (lastBeat < 0.0 || tempo <= 0.0)
		return 0.0;
	return (seconds - lastBeat) * tempo / 60.0;
}
