#include "Screencast.h"

#include <csignal>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

Screencast::~Screencast()
{
	if (child > 0)
	{
		// Quitting while casting: end it as Ctrl+C would and give the script
		// a moment to finish the file.
		stop();
		for (int i = 0; i < 50 && isRunning(); ++i)
			juce::Thread::sleep (100);
		if (child > 0)
			::kill (-child, SIGKILL);
	}
	stopThread (2000);
	if (pipeFd >= 0)
		close (pipeFd);
}

juce::String Screencast::start (const juce::File& script, const juce::String& host)
{
	if (child > 0)
		return {};
	if (! script.existsAsFile())
		return "Nicht gefunden: " + script.getFullPathName();

	stopThread (2000);   // a reader left from the last cast
	if (pipeFd >= 0)
		close (pipeFd);

	int fds[2];
	if (pipe2 (fds, O_CLOEXEC) != 0)
		return "Keine Pipe";

	const auto path = script.getFullPathName().toStdString();
	const auto hostName = host.toStdString();
	const auto preview = std::to_string (frameWidth) + "x" + std::to_string (frameHeight);

	const auto pid = fork();
	if (pid < 0)
	{
		close (fds[0]);
		close (fds[1]);
		return "Konnte den Screencast nicht starten";
	}

	if (pid == 0)
	{
		setsid();   // a process group of its own: stop() reaches ssh, tee and ffmpeg too
		dup2 (fds[1], STDOUT_FILENO);   // dup2 clears close-on-exec for stdout
		setenv ("PREVIEW", preview.c_str(), 1);
		execl ("/bin/bash", "bash", path.c_str(), hostName.c_str(), (char*) nullptr);
		_exit (127);
	}

	close (fds[1]);
	pipeFd = fds[0];
	child = pid;
	startedAt = juce::Time::getMillisecondCounterHiRes() / 1000.0;
	frames = 0;
	{
		const juce::ScopedLock sl (frameLock);
		frame = {};
	}
	startThread();
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

juce::Image Screencast::getFrame() const
{
	const juce::ScopedLock sl (frameLock);
	return frame;
}

void Screencast::run()
{
	// One BGRA frame after the other -- JUCE's ARGB, byte for byte on little-endian.
	std::vector<juce::uint8> buffer ((size_t) (frameWidth * frameHeight * 4));

	while (! threadShouldExit())
	{
		size_t got = 0;
		while (got < buffer.size())
		{
			const auto n = read (pipeFd, buffer.data() + got, buffer.size() - got);
			if (n <= 0)
				return;   // the script ended
			got += (size_t) n;
		}

		juce::Image next (juce::Image::ARGB, frameWidth, frameHeight, false);
		{
			const juce::Image::BitmapData data (next, juce::Image::BitmapData::writeOnly);
			for (int y = 0; y < frameHeight; ++y)
				std::memcpy (data.getLinePointer (y), buffer.data() + (size_t) (y * frameWidth * 4), (size_t) frameWidth * 4);
		}

		const juce::ScopedLock sl (frameLock);
		frame = next;
		++frames;
	}
}
