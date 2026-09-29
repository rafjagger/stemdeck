#include "ProcessGroup.h"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace
{
	std::vector<char*> pointersTo (std::vector<std::string>& strings)
	{
		std::vector<char*> pointers;
		for (auto& s : strings)
			pointers.push_back (s.data());
		pointers.push_back (nullptr);
		return pointers;
	}

	// Everything the child needs is built before fork(): between fork and exec
	// only async-signal-safe calls, no allocation, no environment lock.
	struct ChildPlan
	{
		std::vector<std::string> args, environment;
		std::vector<char*> argv, envp;
		std::string failure;
	};

	ChildPlan planChild (const std::vector<std::string>& argv, const std::vector<std::string>& extraEnvironment)
	{
		ChildPlan plan;
		plan.args = argv;
		for (auto** e = environ; *e != nullptr; ++e)
			plan.environment.emplace_back (*e);
		plan.environment.insert (plan.environment.end(), extraEnvironment.begin(), extraEnvironment.end());
		plan.argv = pointersTo (plan.args);
		plan.envp = pointersTo (plan.environment);
		plan.failure = "cannot run " + argv.front() + "\n";
		return plan;
	}

	[[noreturn]] void becomeChild (const ChildPlan& plan, int output, pid_t parent, bool startStopped)
	{
		setpgid (0, 0);
		prctl (PR_SET_PDEATHSIG, SIGKILL);
		if (getppid() != parent)   // the parent died before prctl
			_exit (1);
		dup2 (output, STDOUT_FILENO);
		dup2 (output, STDERR_FILENO);
		close_range (3, ~0U, 0);
		if (startStopped)
			raise (SIGSTOP);
		execvpe (plan.argv[0], plan.argv.data(), plan.envp.data());
		(void) ::write (STDERR_FILENO, plan.failure.data(), plan.failure.size());
		_exit (127);
	}
}

ProcessGroup::Result ProcessGroup::run (const std::vector<std::string>& argv, const std::vector<std::string>& extraEnvironment,
										const std::function<void (const std::string&)>& onLine, const std::function<bool()>& shouldStop,
										std::string& lastLine)
{
	lastLine.clear();
	if (argv.empty())
		return Result::failed;
	if (shouldStop())
		return Result::stopped;

	const auto plan = planChild (argv, extraEnvironment);
	int out[2];
	if (pipe2 (out, O_CLOEXEC) != 0)
		return (lastLine = "no pipe"), Result::failed;

	pid_t pid = -1;
	{
		// Under the lock, so a pause or resume can't fall between the fork and
		// the moment the group is known.
		std::lock_guard<std::mutex> guard (lock);
		const auto parent = getpid();
		pid = fork();
		if (pid == 0)
			becomeChild (plan, out[1], parent, paused);
		if (pid > 0)
		{
			setpgid (pid, pid);
			group = pid;
		}
	}
	close (out[1]);
	if (pid < 0)
	{
		close (out[0]);
		return (lastLine = "cannot start " + argv.front()), Result::failed;
	}

	bool stopped = false;
	std::string pending;
	char buffer[4096];
	for (;;)
	{
		pollfd fd { out[0], POLLIN, 0 };
		if (poll (&fd, 1, 200) > 0)
		{
			const auto n = read (out[0], buffer, sizeof (buffer));
			if (n <= 0)
				break;
			pending.append (buffer, (size_t) n);
			for (auto end = pending.find_first_of ("\r\n"); end != std::string::npos; end = pending.find_first_of ("\r\n"))
			{
				const auto line = pending.substr (0, end);
				pending.erase (0, end + 1);
				if (line.empty())
					continue;
				if (onLine)
					onLine (line);
				if (line.find ("%|") == std::string::npos)
					lastLine = line;
			}
		}
		if (! stopped && shouldStop())
		{
			stopped = true;
			::kill (-pid, SIGKILL);
		}
	}
	close (out[0]);

	{
		// Forgotten before it is reaped, so no signal goes to a reused id.
		std::lock_guard<std::mutex> guard (lock);
		group = -1;
	}
	int status = 0;
	waitpid (pid, &status, 0);

	if (stopped)
		return Result::stopped;
	return WIFEXITED (status) && WEXITSTATUS (status) == 0 ? Result::ok : Result::failed;
}

void ProcessGroup::setPaused (bool shouldPause)
{
	std::lock_guard<std::mutex> guard (lock);
	if (paused == shouldPause)
		return;
	paused = shouldPause;
	if (group > 0)
		::kill (-group, shouldPause ? SIGSTOP : SIGCONT);
}

void ProcessGroup::kill()
{
	std::lock_guard<std::mutex> guard (lock);
	if (group > 0)
		::kill (-group, SIGKILL);
}
