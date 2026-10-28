// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <filesystem>
#include <optional>

#include "PanelFollowsWorkspace.h"

// Which StemDeck instance holds the A³ Motion panel (-bformat: one instance a
// deck, one panel). A lock file in the runtime directory (cleared at boot) holds
// "<letter> <pid>" of the instance that has it. Pure: no JUCE, the path is a
// parameter.
//
// The instance holding it opens the panel's port; the others never do. A lock
// is free when there is no file, when it is empty or garbled, or when its pid
// is no longer alive (a crashed holder) or belongs to another program (a pid
// recycled after a reboot, or a SIGTERM that never released it). Every change of the lock happens under
// an flock on "<lock>.guard", so two instances starting in the same instant
// cannot both win and a release never removes what another instance took.
namespace panel
{
	// $XDG_RUNTIME_DIR/stemdeck-panel.lock; without it /tmp/stemdeck-<uid>/panel.lock
	// in a folder made 0700 (fallbackBase is a parameter for tests).
	std::filesystem::path lockFilePath (const char* runtimeDir, unsigned uid,
										const std::filesystem::path& fallbackBase = "/tmp");

	class Ownership
	{
	public:
		Ownership (std::filesystem::path lockFile, char deckLetter, int processId = currentProcess());

		bool holds() const;
		// Takes it when it is free. True if this instance holds it afterwards.
		bool claimIfFree();
		// That, for deck A only.
		bool claimByDefault();
		// The "panel: this deck" toggle on: this instance takes it from whoever had it.
		void take();
		// The toggle off, and quit: lets go, but only of what is this instance's.
		void release();

		static int currentProcess();

	private:
		std::filesystem::path file;
		char letter;
		int pid;
	};

	// The start-up decision, from this deck's own stored preference: on claims
	// the panel if free, off never does, absent leaves it to deck A. True if
	// the instance holds it afterwards.
	bool claimAtStart (Ownership&, std::optional<bool> storedPreference);

	// Follow mode (R9, STEMDECK_PANEL_WORKSPACE): the instance starts without
	// the panel and without touching its lock -- two readers never share the
	// tty. Always false.
	bool startFollowing (Ownership&);

	// Acts on the rule's answer and returns whether the instance holds the
	// panel afterwards (the lock's real state, not an assumed one).
	bool applyFollow (Ownership&, PanelFollow::Action);
}
