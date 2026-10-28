// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "RecordTarget.h"

#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

RecordTarget RecordTarget::choose (const std::string& mountPoint, const std::string& localFolder, Stick stick)
{
	if (stick == Stick::writable)
		return { Kind::usb, mountPoint + "/A3-recordings", false };
	return { Kind::local, localFolder, stick == Stick::readOnly };
}

RecordTarget::Stick RecordTarget::stickState (const std::string& mountPoint, const std::string& mountsText)
{
	// An automount line comes first and the real mount over it later: the last line says.
	Stick found = Stick::absent;
	std::istringstream lines (mountsText);
	std::string line;
	while (std::getline (lines, line))
	{
		std::istringstream fields (line);
		std::string device, where, type, options;
		if (! (fields >> device >> where >> type >> options) || where != mountPoint)
			continue;

		bool readOnly = false;
		std::istringstream flags (options);
		for (std::string flag; std::getline (flags, flag, ',');)
			readOnly = readOnly || flag == "ro";
		found = readOnly ? Stick::readOnly : Stick::writable;
	}
	return found;
}

RecordTarget::Stick RecordTarget::stickState (const std::string& mountPoint)
{
	std::ifstream mounts ("/proc/mounts");
	std::stringstream text;
	text << mounts.rdbuf();
	return stickState (mountPoint, text.str());
}

bool RecordTarget::canWrite (const std::string& mountPoint)
{
	return ::access (mountPoint.c_str(), W_OK) == 0;
}

std::int64_t RecordTarget::folderBytes (const std::string& folder)
{
	std::error_code error;
	std::int64_t total = 0;
	for (fs::recursive_directory_iterator it (folder, fs::directory_options::skip_permission_denied, error), end; ! error && it != end; it.increment (error))
	{
		std::error_code sizeError;
		if (it->is_regular_file (sizeError))
			total += (std::int64_t) it->file_size (sizeError);
	}
	return total;
}
