#include "MasterDeck.h"

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
