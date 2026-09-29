#pragma once

#include "ProLinkPackets.h"

// The Pioneer tempo master as a running clock: its tempo, and how far it is
// into the current beat at any moment. Fed with parsed packets and the time
// they arrived; asked with a time. Pure: no sockets, no JUCE, no clock of
// its own, so every answer is about the time it is asked for.
//
// Who leads: the device whose status says it is master and playing. Status
// packets are partly sent to one address, and on the Core NUC StemDeck shares
// that address with beat-analyzer, so they may never arrive here. Without a
// status packet for two seconds there is no master info, and the chosen
// player leads -- or, when none was chosen, the first player that sent a beat.
class PioneerClock
{
public:
	void onBeat (const prolink::BeatPacket& beat, double seconds);
	void onStatus (const prolink::StatusPacket& status, double seconds);

	// The player to follow when there is no master info (1-4).
	void choosePlayer (int device);
	int chosenPlayer() const { return chosen; }

	int leader (double seconds) const;          // 0 = nobody
	bool hasMasterInfo (double seconds) const;  // a status packet within `silence`
	bool isLive (double seconds) const;         // a leader beat within `silence`
	double bpm() const { return tempo; }        // held when silent; 0 = never heard
	double beatPhaseAt (double seconds) const;  // beats since the leader's last beat

	static constexpr double silence = 2.0;

private:
	int master = 0, chosen = 0;
	double lastStatus = -1.0, lastBeat = -1.0, tempo = 0.0;
};
