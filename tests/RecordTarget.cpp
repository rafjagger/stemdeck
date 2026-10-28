// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "RecordTarget.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace
{
fs::path freshFolder (const char* name)
{
	const auto folder = fs::temp_directory_path() / (std::string ("stemdeck-RecordTarget-") + name);
	fs::remove_all (folder);
	fs::create_directories (folder);
	return folder;
}

void writeBytes (const fs::path& file, size_t count)
{
	std::ofstream (file, std::ios::binary) << std::string (count, 'x');
}
}

TEST (RecordTarget, UsesTheStickWhenMountedAndWritable)
{
	const auto target = RecordTarget::choose ("/media/a3-usb", "/home/x/recordings", RecordTarget::Stick::writable);
	EXPECT_EQ (target.kind, RecordTarget::Kind::usb);
	EXPECT_EQ (target.folder, "/media/a3-usb/A3-recordings");
	EXPECT_EQ (target.label(), "USB");
}

TEST (RecordTarget, FallsBackToLocalWhenAbsent)
{
	const auto target = RecordTarget::choose ("/media/a3-usb", "/home/x/recordings", RecordTarget::Stick::absent);
	EXPECT_EQ (target.kind, RecordTarget::Kind::local);
	EXPECT_EQ (target.folder, "/home/x/recordings");
	EXPECT_EQ (target.label(), "LOCAL");
}

// A stick that is there but mounted read-only is not a target, and is not "no stick" either.
TEST (RecordTarget, FallsBackToLocalWhenReadOnly)
{
	const auto target = RecordTarget::choose ("/media/a3-usb", "/home/x/recordings", RecordTarget::Stick::readOnly);
	EXPECT_EQ (target.kind, RecordTarget::Kind::local);
	EXPECT_EQ (target.folder, "/home/x/recordings");
	EXPECT_TRUE (target.stickReadOnly);
	EXPECT_EQ (target.label(), "USB RO");
}

TEST (RecordTarget, ReadsTheStickFromTheMountTableAlone)
{
	const std::string point = "/media/a3-usb";
	using Stick = RecordTarget::Stick;
	EXPECT_EQ (RecordTarget::stickState (point, "other /media/other vfat rw 0 0\n"), Stick::absent);
	EXPECT_EQ (RecordTarget::stickState (point, ""), Stick::absent);
	EXPECT_EQ (RecordTarget::stickState (point, "dev " + point + " vfat ro,relatime 0 0\n"), Stick::readOnly);
	EXPECT_EQ (RecordTarget::stickState (point, "dev " + point + " vfat rw,relatime 0 0\n"), Stick::writable);
	// systemd's automount line first, the real mount over it later: the last one says.
	EXPECT_EQ (RecordTarget::stickState (point, "systemd-1 " + point + " autofs rw,fd=1 0 0\n/dev/sdb1 " + point + " vfat ro 0 0\n"),
			   Stick::readOnly);
	EXPECT_EQ (RecordTarget::stickState (point, "systemd-1 " + point + " autofs rw,fd=1 0 0\n"), Stick::writable)
		<< "not mounted yet: present, and not ours to wake up";
}

TEST (RecordTarget, RefusesLocalAtTheLimit)
{
	EXPECT_EQ (RecordTarget::localLimitBytes, 100'000'000'000);
	EXPECT_TRUE (RecordTarget::localHasRoom (0));
	EXPECT_TRUE (RecordTarget::localHasRoom (RecordTarget::localLimitBytes - 1));
	EXPECT_FALSE (RecordTarget::localHasRoom (RecordTarget::localLimitBytes));
	EXPECT_FALSE (RecordTarget::localHasRoom (RecordTarget::localLimitBytes + 1));
	EXPECT_FALSE (RecordTarget::localHasRoom (1000, 1000)) << "the limit can be given";
}

TEST (RecordTarget, MeasuresAFolderRecursively)
{
	const auto folder = freshFolder ("size");
	fs::create_directories (folder / "sub");
	writeBytes (folder / "a.flac", 1000);
	writeBytes (folder / "sub" / "b.flac", 234);
	EXPECT_EQ (RecordTarget::folderBytes (folder.string()), 1234);
	EXPECT_EQ (RecordTarget::folderBytes ((folder / "missing").string()), 0);
	fs::remove_all (folder);
}

// The recordings are the maintainer's: asking, measuring and choosing never touch a file.
TEST (RecordTarget, NeverDeletesAnything)
{
	const auto folder = freshFolder ("keep");
	writeBytes (folder / "a.flac", 500);
	writeBytes (folder / "b.flac", 500);

	const auto bytes = RecordTarget::folderBytes (folder.string());
	EXPECT_FALSE (RecordTarget::localHasRoom (RecordTarget::localLimitBytes + bytes));
	RecordTarget::choose ("/media/a3-usb", folder.string(), RecordTarget::Stick::absent);
	RecordTarget::stickState (folder.string(), "");
	RecordTarget::canWrite (folder.string());

	EXPECT_TRUE (fs::exists (folder / "a.flac"));
	EXPECT_TRUE (fs::exists (folder / "b.flac"));
	EXPECT_EQ (RecordTarget::folderBytes (folder.string()), 1000);
	fs::remove_all (folder);
}
