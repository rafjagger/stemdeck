// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "SpawnOutput.h"

#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace
{
using Clock = std::chrono::steady_clock;

// posix_spawn shares the caller's memory until exec (CLONE_VM | CLONE_VFORK
// in glibc): nothing is copied, whatever the caller holds.
pid_t spawnWithStdoutTo (const std::vector<std::string>& argv, int writeEnd)
{
	std::vector<char*> args;
	for (const auto& word : argv)
		args.push_back (const_cast<char*> (word.c_str()));
	args.push_back (nullptr);

	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init (&actions);
	posix_spawn_file_actions_adddup2 (&actions, writeEnd, STDOUT_FILENO);
	posix_spawn_file_actions_addopen (&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

	pid_t pid = -1;
	const auto failed = posix_spawnp (&pid, args[0], &actions, nullptr, args.data(), environ) != 0;
	posix_spawn_file_actions_destroy (&actions);
	return failed ? -1 : pid;
}

// Reads until the child closes its end; false when the deadline came first.
bool readUntilClosed (int readEnd, Clock::time_point deadline, std::string& output)
{
	char chunk[4096];
	for (;;)
	{
		const auto left = std::chrono::duration_cast<std::chrono::milliseconds> (deadline - Clock::now()).count();
		if (left <= 0)
			return false;

		pollfd fd { readEnd, POLLIN, 0 };
		if (poll (&fd, 1, (int) left) <= 0)
			continue;

		const auto got = read (readEnd, chunk, sizeof (chunk));
		if (got == 0)
			return true;
		if (got > 0)
			output.append (chunk, (size_t) got);
	}
}
}

std::string spawnForOutput (const std::vector<std::string>& argv, int timeoutMs)
{
	if (argv.empty())
		return {};

	int ends[2];
	if (pipe2 (ends, O_CLOEXEC) != 0)
		return {};

	const auto pid = spawnWithStdoutTo (argv, ends[1]);
	close (ends[1]);
	if (pid < 0)
	{
		close (ends[0]);
		return {};
	}

	std::string output;
	const auto finished = readUntilClosed (ends[0], Clock::now() + std::chrono::milliseconds (timeoutMs), output);
	close (ends[0]);
	if (! finished)
		kill (pid, SIGKILL);

	int status = 0;
	waitpid (pid, &status, 0);
	return finished && WIFEXITED (status) && WEXITSTATUS (status) == 0 ? output : std::string();
}
