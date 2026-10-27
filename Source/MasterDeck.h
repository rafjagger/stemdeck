#pragma once

#include <array>
#include <map>

// Which deck is StemDeck's tempo master -- the one whose beat goes out to the
// Pioneer network, like a CDJ's MASTER. -1 for none.
//   - a pressed MASTER (0 or 1) wins, and hands over from the other deck;
//   - a master that stops while the other deck plays hands over to it -- as
//     on a CDJ-3000, where pausing the sync master changes it (manual p. 71);
//   - otherwise the current master stays (with neither deck playing it
//     stays master and simply sends no beats);
//   - pressing MASTER on the master turns it off;
//   - with none yet and `automatic` allowed, the only playing deck becomes
//     master; two playing and none chosen stays none -- a choice for a hand.
int chooseMaster (int pressed, std::array<bool, 2> playing, int current, bool automatic);

// One deck per instance (-bformat, S4.7): MASTER across instances on the link,
// where no other deck in this process can take it over. Pressed, the deck
// publishes; pressed again, it stops. The newest claim wins: a master that
// hears another player start claiming master after its own press gives it
// up -- the handover the link does not do for us (stemdeck#3). A claim
// already running when this instance joined the link is older than any press
// here, even if it is first heard a moment after the join and the join came
// after the press. A claim not heard
// for `silence` is forgotten. Pure: the times are the caller's.
class InstanceMaster
{
public:
	void joined (double seconds);   // this instance started listening on the link
	void press (double seconds);    // MASTER pressed: on, or off when on
	void onStatus (int device, bool claimsMaster, double seconds);

	// True once, when another player's newer claim takes master from this deck.
	bool yieldIfOvertaken (double seconds);
	bool isMaster() const { return master; }

	static constexpr double joinGrace = 0.5;   // > the 200 ms between status packets
	static constexpr double silence = 2.0;

private:
	struct Claim
	{
		double since = 0.0, last = 0.0;
	};

	bool master = false;
	double claimedAt = 0.0, joinedAt = -1.0e9;
	std::map<int, Claim> claims;
};
