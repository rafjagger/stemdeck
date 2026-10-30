// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "StemColumn.h"

#include <functional>

// The panel's four pots as the stems' AUX knobs, absolute (2026-10-29): a
// reading is the knob's value at once, whatever the screen set before -- the
// deck wins, there is no pick-up. Message thread only.
class AuxPots
{
public:
	static constexpr int count = 4;
	explicit AuxPots (std::function<StemColumn&(int)> columnOf) : column (std::move (columnOf)) {}

	// A pot's reading, 0..1; a pot past the four is ignored.
	void reading (int stem, double value);

private:
	std::function<StemColumn&(int)> column;
};
