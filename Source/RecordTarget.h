// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string>

// Where a take goes: onto the USB stick when one is mounted and writable,
// else into the local recordings folder, which is capped. Pure, apart from
// the two functions that read the mount table and a folder; none of them
// ever changes or removes a file.
struct RecordTarget
{
	enum class Kind { usb, local };
	// What the mount table says about the stick; read-only is "present, but not a target".
	enum class Stick { absent, readOnly, writable };

	// Maintainer, 2026-10-27: the local recordings may hold at most 100 GB.
	// The stick has no limit of ours beyond its free space.
	static constexpr std::int64_t localLimitBytes = 100'000'000'000;

	Kind kind = Kind::local;
	std::string folder;
	bool stickReadOnly = false;   // a stick is there, but cannot be written: the take goes local

	std::string label() const { return kind == Kind::usb ? "USB" : stickReadOnly ? "USB RO" : "LOCAL"; }

	static RecordTarget choose (const std::string& mountPoint, const std::string& localFolder, Stick stick);

	// Reads the mount table only (`mountsText` is /proc/mounts; the one-argument
	// form reads the file) and never touches the mount point: access() on an
	// automount counts as activity and would keep the stick from idling out.
	static Stick stickState (const std::string& mountPoint, const std::string& mountsText);
	static Stick stickState (const std::string& mountPoint);
	// For the moment a take starts: can a file be created there right now?
	static bool canWrite (const std::string& mountPoint);

	static bool localHasRoom (std::int64_t folderBytes, std::int64_t limit = localLimitBytes) { return folderBytes < limit; }
	static std::int64_t folderBytes (const std::string& folder);   // 0 when it does not exist
};
