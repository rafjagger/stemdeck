// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>

// Two presses of one key within windowSeconds make a double; the double uses
// both up, so a third quick press starts again. The time comes from the caller.
class DoublePress
{
public:
	static constexpr double windowSeconds = 0.4;

	bool press (double nowSeconds)
	{
		const bool isDouble = previous && nowSeconds - *previous <= windowSeconds;
		previous = isDouble ? std::nullopt : std::optional<double> (nowSeconds);
		return isDouble;
	}

private:
	std::optional<double> previous;
};
