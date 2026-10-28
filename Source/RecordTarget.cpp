// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "RecordTarget.h"

#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

RecordTarget RecordTarget::choose (const std::string& mountPoint, const std::string& localFolder, bool isMountedAndWritable)
{
	if (isMountedAndWritable)
		return { Kind::usb, mountPoint + "/A3-recordings" };
	return { Kind::local, localFolder };
}

bool RecordTarget::isMountedAndWritable (const std::string& mountPoint, const std::string& mountsText)
{
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
		// access() also triggers an automount, and says no on a read-only one.
		return ! readOnly && ::access (mountPoint.c_str(), W_OK) == 0;
	}
	return false;
}

bool RecordTarget::isMountedAndWritable (const std::string& mountPoint)
{
	std::ifstream mounts ("/proc/mounts");
	std::stringstream text;
	text << mounts.rdbuf();
	return isMountedAndWritable (mountPoint, text.str());
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
