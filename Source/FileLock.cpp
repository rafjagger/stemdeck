// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "FileLock.h"

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

FileLock::FileLock (const juce::File& lockFile) : file (lockFile) {}

FileLock::~FileLock()
{
	release();
}

FileLock::Result FileLock::tryAcquire() { return take (false); }
FileLock::Result FileLock::acquire() { return take (true); }

FileLock::Result FileLock::take (bool wait)
{
	if (descriptor >= 0)
		return Result::acquired;

	const auto fd = ::open (file.getFullPathName().toRawUTF8(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
	if (fd < 0)
		return Result::unavailable;
	if (::flock (fd, wait ? LOCK_EX : LOCK_EX | LOCK_NB) != 0)
	{
		::close (fd);
		return wait ? Result::unavailable : Result::heldElsewhere;
	}
	descriptor = fd;
	return Result::acquired;
}

void FileLock::release()
{
	if (descriptor < 0)
		return;
	::flock (descriptor, LOCK_UN);
	::close (descriptor);
	descriptor = -1;
}
