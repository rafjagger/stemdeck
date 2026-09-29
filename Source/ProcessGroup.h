#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include <sys/types.h>

// Runs one command at a time as its own process group, so one signal reaches
// everything it starts. Pure POSIX, no JUCE: testable.
//
// The child dies with the thread that started it (PR_SET_PDEATHSIG), and it
// inherits no descriptors but stdout/stderr -- a crashed StemDeck leaves no
// separator on CPU 0 and no one holding its Pro DJ Link sockets.
class ProcessGroup
{
public:
	enum class Result { ok, failed, stopped };

	// `extraEnvironment` ("NAME=value") is added to the inherited one. Output
	// (stdout and stderr) arrives line by line; '\r' ends a line too (tqdm).
	// `shouldStop` is asked before the start and about every 200 ms; true
	// kills the group. `lastLine` is the last line that was not progress.
	Result run (const std::vector<std::string>& argv, const std::vector<std::string>& extraEnvironment,
				const std::function<void (const std::string&)>& onLine, const std::function<bool()>& shouldStop,
				std::string& lastLine);

	// Stopped, never killed; remembered for the next run.
	void setPaused (bool shouldPause);
	// Kills the running group, if any.
	void kill();

private:
	std::mutex lock;
	pid_t group = -1;
	bool paused = false;
};
