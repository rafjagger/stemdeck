#include "MasterDeck.h"

int chooseMaster (int pressed, std::array<bool, 2> playing, int current)
{
	if (pressed == 0 || pressed == 1)
		return pressed;

	if (current == 0 || current == 1)
		return current;

	if (playing[0] != playing[1])
		return playing[0] ? 0 : 1;

	return -1;
}
