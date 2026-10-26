// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "PanelProtocol.h"

#include <array>

// What every panel map shares, whatever the keys mean: a key's press and
// release from its two bits, and which of the 44 keys and 8 encoder pushes
// were down at the last poll. Pure.
namespace panel
{
	// The firmware's states, read as A³ Motion reads them.
	struct Edges { bool pressed = false, released = false; };

	inline Edges edges (KeyState state, bool& down)
	{
		Edges e;
		switch (state)
		{
			case KeyState::click:
				e.pressed = ! down;
				e.released = true;
				down = false;
				break;
			case KeyState::held:
				e.pressed = ! down;
				down = true;
				break;
			case KeyState::idle:
				e.released = down;
				down = false;
				break;
			case KeyState::bounce:
				break;
		}
		return e;
	}

	class EdgeTracker
	{
	public:
		Edges key (int button, KeyState state) { return edges (state, keyDown[(size_t) button]); }
		Edges push (int encoder, KeyState state) { return edges (state, pushDown[(size_t) encoder]); }
		// A panel (re)connected: every key up.
		void reset() { keyDown = {}; pushDown = {}; }

	private:
		std::array<bool, buttonCount> keyDown {};
		std::array<bool, encoderCount> pushDown {};
	};
}
