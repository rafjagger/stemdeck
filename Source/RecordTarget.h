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

	// Maintainer, 2026-10-27: the local recordings may hold at most 100 GB.
	// The stick has no limit of ours beyond its free space.
	static constexpr std::int64_t localLimitBytes = 100'000'000'000;

	Kind kind = Kind::local;
	std::string folder;

	std::string label() const { return kind == Kind::usb ? "USB" : "LOCAL"; }

	static RecordTarget choose (const std::string& mountPoint, const std::string& localFolder, bool isMountedAndWritable);

	// `mountsText` is the content of /proc/mounts; the one-argument form reads it.
	static bool isMountedAndWritable (const std::string& mountPoint, const std::string& mountsText);
	static bool isMountedAndWritable (const std::string& mountPoint);

	static bool localHasRoom (std::int64_t folderBytes) { return folderBytes < localLimitBytes; }
	static std::int64_t folderBytes (const std::string& folder);   // 0 when it does not exist
};
