#pragma once

#include <array>

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
