// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

// Who a one-deck instance is: everything that must differ between two
// instances on one machine hangs off the letter.
struct DeckInstance
{
	char letter;

	std::string jackClientName() const; // "StemDeck-A"
	std::string sessionDir() const;     // "A", under ~/.config/StemDeck/
	int oscDeck() const;                // A -> 1, B -> 2 ... Z -> 26 (the truth's {deck})
	std::string title() const;          // "StemDeck A"
	// Its player number on the link: two instances on one host share the
	// settings file, and a receiver drops its own number's beats. A is 6 (the
	// two-deck app's), B is 5, C 4, D 3: inside the CDJ-3000's 1-6 (manual
	// p. 113). A letter beyond D has none: 0, and the instance stays off the
	// link.
	int proLinkDevice() const;
};
