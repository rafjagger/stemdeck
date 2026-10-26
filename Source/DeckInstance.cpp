// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "DeckInstance.h"

std::string DeckInstance::jackClientName() const { return std::string ("StemDeck-") + letter; }
std::string DeckInstance::sessionDir() const { return std::string (1, letter); }
int DeckInstance::oscDeck() const { return letter - 'A' + 1; }
std::string DeckInstance::title() const { return std::string ("StemDeck ") + letter; }
