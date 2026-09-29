#pragma once

#include <array>

// Which deck is StemDeck's tempo master -- the one whose beat goes out to the
// Pioneer network, like a CDJ's MASTER. -1 for none.
//   - a pressed MASTER (0 or 1) wins, and hands over from the other deck;
//   - otherwise the current master stays, playing or not (as on a CDJ: a
//     stopped master sends no beats, but is still master);
//   - with none yet, the only playing deck becomes master; two playing and
//     none chosen stays none -- that is a choice for a hand.
int chooseMaster (int pressed, std::array<bool, 2> playing, int current);
