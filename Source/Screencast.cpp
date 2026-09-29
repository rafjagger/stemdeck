#include "Screencast.h"

#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

Screencast::~Screencast()
{
	if (child <= 0)
		return;

	// Quitting while casting: end it as Ctrl+C would and give the script a
	// moment to finish the file.
	stop();
	for (int i = 0; i < 50 && isRunning(); ++i)
		juce::Thread::sleep (100);
	if (child > 0)
		::kill (-child, SIGKILL);
}

juce::String Screencast::start (const juce::File& script, const juce::String& host, bool liveView)
{
	if (child > 0)
		return {};
	if (! script.existsAsFile())
		return "Nicht gefunden: " + script.getFullPathName();

	const auto path = script.getFullPathName().toStdString();
	const auto hostName = host.toStdString();

	const auto pid = fork();
	if (pid < 0)
		return "Konnte den Screencast nicht starten";

	if (pid == 0)
	{
		setsid();   // a process group of its own: stop() reaches ssh, tee and ffplay too
		if (! liveView)
			setenv ("NOVIEW", "1", 1);
		execl ("/bin/bash", "bash", path.c_str(), hostName.c_str(), (char*) nullptr);
		_exit (127);
	}

	child = pid;
	startedAt = juce::Time::getMillisecondCounterHiRes() / 1000.0;
	return {};
}

void Screencast::stop()
{
	if (child > 0)
		::kill (-child, SIGINT);
}

bool Screencast::isRunning()
{
	if (child <= 0)
		return false;

	int status = 0;
	if (waitpid (child, &status, WNOHANG) == child)
		child = -1;
	return child > 0;
}

double Screencast::getSeconds() const
{
	return child > 0 ? juce::Time::getMillisecondCounterHiRes() / 1000.0 - startedAt : 0.0;
}
