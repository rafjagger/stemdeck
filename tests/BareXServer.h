// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <dlfcn.h>

#include <cstdlib>

// Tests that open a real window run under xvfb-run (tests/CMakeLists.txt).
namespace barex
{
	inline bool hasDisplay() { return std::getenv ("DISPLAY") != nullptr && *std::getenv ("DISPLAY") != 0; }

	// A bare Xvfb has no window manager, so the atoms JUCE only looks up
	// (WM_PROTOCOLS, _NET_WM_WINDOW_TYPE, ...) do not exist, and its first
	// window dies with BadAtom. Creating them, as a window manager would,
	// is what makes the bare server usable. Atoms live as long as one client is connected.
	inline void internWindowManagerAtoms()
	{
		void* x11 = dlopen ("libX11.so.6", RTLD_NOW);
		if (x11 == nullptr)
			return;
		using Open = void* (*) (const char*);
		using Intern = unsigned long (*) (void*, const char*, int);
		const auto open = reinterpret_cast<Open> (dlsym (x11, "XOpenDisplay"));
		const auto intern = reinterpret_cast<Intern> (dlsym (x11, "XInternAtom"));
		if (open == nullptr || intern == nullptr)
			return;
		if (auto* display = open (nullptr))
		{
			for (const char* name : { "WM_PROTOCOLS", "WM_TAKE_FOCUS", "WM_DELETE_WINDOW", "_NET_WM_PING",
								 "_NET_WM_WINDOW_TYPE", "_NET_WM_STATE" })
				intern (display, name, 0);
			// Kept open: Xvfb resets, atoms included, when its last client leaves.
			static void* keepAlive = display;
			(void) keepAlive;
		}
	}
}
