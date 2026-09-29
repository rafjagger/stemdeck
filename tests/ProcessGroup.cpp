#include <gtest/gtest.h>

#include "ProcessGroup.h"

#include <chrono>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
	using Clock = std::chrono::steady_clock;

	struct Run
	{
		ProcessGroup::Result result;
		std::vector<std::string> lines;
		std::string lastLine;
	};

	Run runShell (ProcessGroup& group, const std::string& script, std::function<bool()> shouldStop = [] { return false; },
				  std::vector<std::string> environment = {})
	{
		Run r;
		r.result = group.run ({ "sh", "-c", script }, environment,
							  [&r] (const std::string& line) { r.lines.push_back (line); }, shouldStop, r.lastLine);
		return r;
	}

	bool contains (const std::vector<std::string>& lines, const std::string& line)
	{
		return std::find (lines.begin(), lines.end(), line) != lines.end();
	}
}

TEST (ProcessGroup, ARunReportsItsLines)
{
	ProcessGroup group;
	const auto r = runShell (group, "printf 'a\\rb\\nc\\n'");
	EXPECT_EQ (r.result, ProcessGroup::Result::ok);
	EXPECT_EQ (r.lines, (std::vector<std::string> { "a", "b", "c" }));
}

TEST (ProcessGroup, AFailingCommandSaysWhy)
{
	ProcessGroup group;
	const auto r = runShell (group, "echo boom; exit 3");
	EXPECT_EQ (r.result, ProcessGroup::Result::failed);
	EXPECT_EQ (r.lastLine, "boom");
}

TEST (ProcessGroup, AMissingProgramSaysSo)
{
	ProcessGroup group;
	std::string last;
	const auto result = group.run ({ "/nonexistent/demucs" }, {}, {}, [] { return false; }, last);
	EXPECT_EQ (result, ProcessGroup::Result::failed);
	EXPECT_NE (last.find ("cannot run /nonexistent/demucs"), std::string::npos) << last;
}

TEST (ProcessGroup, TheExtraEnvironmentReachesTheChild)
{
	ProcessGroup group;
	const auto r = runShell (group, "echo $STEMDECK_TEST", [] { return false; }, { "STEMDECK_TEST=bar" });
	EXPECT_TRUE (contains (r.lines, "bar"));
}

TEST (ProcessGroup, StopKillsTheWholeGroup)
{
	// The background sleep keeps the pipe open: killing only the shell
	// would leave the run waiting 30 s for it.
	ProcessGroup group;
	const auto start = Clock::now();
	const auto r = runShell (group, "sleep 30 & sleep 30",
							 [start] { return Clock::now() - start > std::chrono::milliseconds (300); });
	EXPECT_EQ (r.result, ProcessGroup::Result::stopped);
	EXPECT_LT (Clock::now() - start, std::chrono::seconds (3));
}

TEST (ProcessGroup, AStopBeforeTheStartRunsNothing)
{
	ProcessGroup group;
	const auto marker = "/tmp/stemdeck-processgroup-" + std::to_string (getpid());
	const auto r = runShell (group, "touch " + marker, [] { return true; });
	EXPECT_EQ (r.result, ProcessGroup::Result::stopped);
	EXPECT_NE (access (marker.c_str(), F_OK), 0);
	unlink (marker.c_str());
}

TEST (ProcessGroup, TheChildInheritsNoDescriptors)
{
	const auto sock = socket (AF_INET, SOCK_DGRAM, 0);   // like a Pro DJ Link socket: no CLOEXEC
	const auto high = fcntl (sock, F_DUPFD, 100);
	close (sock);
	ASSERT_GE (high, 100);

	ProcessGroup group;
	const auto r = runShell (group, "ls /proc/self/fd");
	close (high);
	EXPECT_FALSE (contains (r.lines, std::to_string (high)));
}

TEST (ProcessGroup, APausedGroupStartsStoppedAndResumes)
{
	ProcessGroup group;
	group.setPaused (true);
	const auto start = Clock::now();
	bool resumed = false;
	const auto r = runShell (group, "echo started", [&]
	{
		if (Clock::now() - start > std::chrono::milliseconds (500) && ! resumed)
		{
			// Nothing may have run while paused.
			resumed = true;
			group.setPaused (false);
		}
		return Clock::now() - start > std::chrono::seconds (5);
	});
	EXPECT_EQ (r.result, ProcessGroup::Result::ok);
	EXPECT_TRUE (resumed);
	EXPECT_GE (Clock::now() - start, std::chrono::milliseconds (500));
	EXPECT_TRUE (contains (r.lines, "started"));
}
