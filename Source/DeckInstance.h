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
	int oscDeck() const;                // A -> 1, B -> 2 (the truth's {deck} range)
	std::string title() const;          // "StemDeck A"
};
