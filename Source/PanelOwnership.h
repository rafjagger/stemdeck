// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <filesystem>

// Which StemDeck instance holds the A³ Motion panel (-bformat: one instance a
// deck, one panel). A lock file, ~/.config/StemDeck/panel.lock, holds the
// letter of the instance that has it. Pure: no JUCE, the path is a parameter.
//
// The instance holding it opens the panel's port; the others never do. The
// panel's port is also taken exclusively (PanelDevice), so a stale lock after
// a crash costs a toggle on the instance that should have it, not a clash.
namespace panel
{
	class Ownership
	{
	public:
		Ownership (std::filesystem::path lockFile, char deckLetter);

		bool holds() const;
		// Takes it when nobody holds it (no lock, or an empty one). True if this instance holds it afterwards.
		bool claimIfFree();
		// Start-up default: that, for deck A only.
		bool claimByDefault();
		// The "panel: this deck" toggle on: this instance takes it from whoever had it.
		void take();
		// The toggle off, and quit: lets go, but only of what is this instance's.
		void release();

	private:
		std::filesystem::path file;
		char letter;
	};
}
