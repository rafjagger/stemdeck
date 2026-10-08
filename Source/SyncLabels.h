#pragma once

#include <string>

// What the network sync UI says (stemdeck#4). It names the protocol's role --
// a link, players, a master -- never a maker or a product line: StemDeck works
// with Pro DJ Link, it is not one of its products (see the README's notice).
// The stored setting keeps its old value "pio"; only what is shown changed.
namespace syncLabels
{
	std::string sourceButton (bool network);
	std::string player (int number);

	// The status line while SYNC follows the network.
	std::string following (double bpm, int leader); // leader 0: the master is silent, its tempo held
	std::string nothingHeard();
	std::string noMaster();
	std::string error (const std::string& why);

	// A deck of ours gives the beat to the network.
	std::string sendingMaster (int deck);

	std::string masterTooltip();
	std::string gridTooltip();
}
