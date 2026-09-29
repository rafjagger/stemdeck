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
	if (soundReader.joinable())
		soundReader.join();   // ends with the pipe, as the script does
	if (pipeFd >= 0)
		close (pipeFd);
	if (soundFd >= 0)
		close (soundFd);
}

juce::String Screencast::start (const juce::File& script, const juce::String& host)
{
	if (child > 0)
		return {};
	if (! script.existsAsFile())
		return "Nicht gefunden: " + script.getFullPathName();

	stopThread (2000);   // readers left from the last cast
	if (soundReader.joinable())
		soundReader.join();
	for (auto* fd : { &pipeFd, &soundFd })
		if (*fd >= 0)
		{
			close (*fd);
			*fd = -1;
		}

	int fds[2], sound[2];
	if (pipe2 (fds, O_CLOEXEC) != 0)
		return "Keine Pipe";
	if (pipe2 (sound, O_CLOEXEC) != 0)
	{
		close (fds[0]);
		close (fds[1]);
		return "Keine Pipe";
	}

	const auto path = script.getFullPathName().toStdString();
	const auto hostName = host.toStdString();
	const auto preview = std::to_string (frameWidth) + "x" + std::to_string (frameHeight);

	const auto pid = fork();
	if (pid < 0)
	{
		for (auto fd : { fds[0], fds[1], sound[0], sound[1] })
			close (fd);
		return "Konnte den Screencast nicht starten";
	}

	if (pid == 0)
	{
		setsid();   // a process group of its own: stop() reaches ssh, tee and ffmpeg too
		dup2 (fds[1], STDOUT_FILENO);   // dup2 clears close-on-exec for these two
		if (sound[1] == 3)
			fcntl (3, F_SETFD, 0);   // dup2 onto itself would leave close-on-exec set
		else
			dup2 (sound[1], 3);
		setenv ("PREVIEW", preview.c_str(), 1);
		execl ("/bin/bash", "bash", path.c_str(), hostName.c_str(), (char*) nullptr);
		_exit (127);
	}

	close (fds[1]);
	close (sound[1]);
	pipeFd = fds[0];
	soundFd = sound[0];
	child = pid;
	startedAt = juce::Time::getMillisecondCounterHiRes() / 1000.0;
	frames = 0;
	{
		const juce::ScopedLock sl (frameLock);
		frame = {};
	}
	startThread();
	soundReader = std::thread ([this] { readSound(); });
	return {};
}

void Screencast::readSound()
{
	// Interleaved stereo s16: the peak of whatever came, per channel.
	std::array<juce::int16, 512> buffer;
	for (;;)
	{
		const auto n = read (soundFd, buffer.data(), sizeof (buffer));
		if (n <= 0)
			return;
		std::array<float, 2> peak {};
		for (size_t i = 0; i + 1 < (size_t) n / sizeof (juce::int16); i += 2)
			for (size_t c = 0; c < 2; ++c)
				peak[c] = juce::jmax (peak[c], std::abs ((float) buffer[i + c]) / 32768.0f);
		for (size_t c = 0; c < 2; ++c)
			if (peak[c] > peaks[c].load())
				peaks[c] = peak[c];
	}
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
