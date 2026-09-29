#include "StemCreator.h"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

StemCreator::StemCreator() : juce::Thread ("Stem creator")
{
	venvFolder = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".local/share/StemDeck/separator");
	startThread (juce::Thread::Priority::low);
}

StemCreator::~StemCreator()
{
	signalThreadShouldExit();
	cancelRequested = true;
	signalChild (SIGKILL);   // nothing of a job outlives StemDeck; SIGKILL reaches stopped processes too
	wake.signal();
	stopThread (5000);
}

void StemCreator::setLibraryFolder (const juce::File& folder)
{
	std::lock_guard<std::mutex> guard (lock);
	libraryFolder = folder;
}

void StemCreator::setVenv (const juce::File& venv)
{
	std::lock_guard<std::mutex> guard (lock);
	venvFolder = venv;
}

int StemCreator::add (const juce::File& input, const juce::String& artist, const juce::String& album, const juce::String& track)
{
	int id = 0;
	{
		std::lock_guard<std::mutex> guard (lock);
		lastError.clear();
		id = queue.add (input.getFullPathName().toStdString(), artist.toStdString(), album.toStdString(), track.toStdString());
	}
	wake.signal();
	return id;
}

void StemCreator::cancel (int id)
{
	std::lock_guard<std::mutex> guard (lock);
	if (queue.cancel (id) == StemJobQueue::Cancel::kill)
	{
		cancelRequested = true;
		signalChild (SIGKILL);
	}
}

void StemCreator::cancelRunning()
{
	std::optional<int> id;
	{
		std::lock_guard<std::mutex> guard (lock);
		id = queue.running();
	}
	if (id)
		cancel (*id);
}

void StemCreator::setPaused (bool shouldPause)
{
	if (paused.exchange (shouldPause) == shouldPause)
		return;
	signalChild (shouldPause ? SIGSTOP : SIGCONT);
}

StemCreator::Status StemCreator::status() const
{
	std::lock_guard<std::mutex> guard (lock);
	Status s;
	s.running = queue.running().has_value();
	s.paused = paused.load();
	s.track = runningTrack;
	s.progress = progress;
	s.lastError = lastError;
	for (const auto& e : queue.jobs())
		if (e.state == JobState::queued)
			++s.waiting;
	return s;
}

void StemCreator::signalChild (int signal)
{
	if (const auto group = childGroup.load(); group > 0)
		::kill (-group, signal);
}

void StemCreator::run()
{
	while (! threadShouldExit())
	{
		std::optional<StemJobEntry> job;
		{
			std::lock_guard<std::mutex> guard (lock);
			if (const auto id = queue.startNext())
			{
				job = *queue.find (*id);
				runningTrack = juce::String (job->track);
				progress = 0.0;
			}
		}

		if (! job)
		{
			wake.wait (500);
			continue;
		}

		cancelRequested = false;
		juce::String error;
		const auto ok = runJob (*job, error);

		std::lock_guard<std::mutex> guard (lock);
		if (! cancelRequested)
		{
			queue.finished (job->id, ok, error.toStdString());
			if (! ok)
				lastError = juce::String (job->track) + ": " + error;
		}
		runningTrack.clear();
		progress = 0.0;
	}
}

bool StemCreator::runJob (const StemJobEntry& job, juce::String& error)
{
	juce::File library, venv;
	{
		std::lock_guard<std::mutex> guard (lock);
		library = libraryFolder;
		venv = venvFolder;
	}

	const juce::File input (job.input);
	if (! input.existsAsFile())
		return (error = "the file is gone"), false;
	if (! venv.getChildFile ("bin/demucs").existsAsFile())
		return (error = "separator not installed (tools/setup-separator.sh)"), false;

	// Staging outside the library, so no half-made set is ever scanned.
	const auto staging = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
							 .getChildFile (".cache/StemDeck/jobs/" + juce::String (job.id));
	staging.deleteRecursively();
	staging.createDirectory();
	const auto cleanUp = [&staging] { staging.deleteRecursively(); };

	const auto wav = staging.getChildFile ("input.wav").getFullPathName().toStdString();
	if (! execute (decodeCommand (input.getFullPathName().toStdString(), wav), {}, {}, error))
		return cleanUp(), false;

	const auto onLine = [this] (const std::string& line)
	{
		if (const auto p = parseDemucsProgress (line))
		{
			std::lock_guard<std::mutex> guard (lock);
			progress = *p;
		}
	};
	if (! execute (separateCommand (venv.getFullPathName().toStdString(), wav, staging.getFullPathName().toStdString(), fast),
				   separateEnvironment (fast), onLine, error))
		return cleanUp(), false;

	const auto plan = planStemJob (library.getFullPathName().toStdString(), job.artist, job.album, job.track,
								   input.getFileName().toStdString(),
								   [] (const std::string& path) { return juce::File (path).exists(); });

	juce::File (plan.albumFolder).createDirectory();
	for (int stem = 0; stem < 4; ++stem)
	{
		const juce::File from (demucsOutputFile (staging.getFullPathName().toStdString(), wav, stem));
		if (! from.moveFileTo (juce::File (plan.stemPaths[(size_t) stem])))
		{
			for (int s = 0; s < stem; ++s)
				juce::File (plan.stemPaths[(size_t) s]).deleteFile();  // no three-stem set
			error = "could not move " + from.getFileName();
			return cleanUp(), false;
		}
	}

	const juce::File original (plan.originalPath);
	original.getParentDirectory().createDirectory();
	input.copyFileTo (original);  // copied: the source may be on a stick or still in use
	cleanUp();

	const juce::File firstStem (plan.stemPaths[0]);
	juce::MessageManager::callAsync ([this, firstStem] { if (onSetCreated) onSetCreated (firstStem); });
	return true;
}

bool StemCreator::execute (const std::vector<std::string>& argv, const std::vector<std::string>& environment,
						   const std::function<void (const std::string&)>& onLine, juce::String& error)
{
	int out[2];
	if (pipe (out) != 0)
		return (error = "no pipe"), false;

	std::vector<char*> args;
	for (const auto& a : argv)
		args.push_back (const_cast<char*> (a.c_str()));
	args.push_back (nullptr);

	const auto pid = fork();
	if (pid < 0)
	{
		close (out[0]);
		close (out[1]);
		return (error = "cannot start " + juce::String (argv[0])), false;
	}

	if (pid == 0)
	{
		// Its own process group, so one signal reaches everything it starts.
		setpgid (0, 0);
		dup2 (out[1], STDOUT_FILENO);
		dup2 (out[1], STDERR_FILENO);
		close (out[0]);
		close (out[1]);
		for (const auto& e : environment)
			putenv (const_cast<char*> (e.c_str()));
		execvp (args[0], args.data());
		const auto message = std::string ("cannot run ") + args[0] + ": " + std::strerror (errno) + "\n";
		(void) ::write (STDERR_FILENO, message.data(), message.size());
		_exit (127);
	}

	setpgid (pid, pid);
	childGroup = pid;
	if (paused)
		::kill (-pid, SIGSTOP);
	close (out[1]);

	std::string pending, lastLine;
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
			// tqdm ends its updates with '\r', everything else with '\n'.
			for (auto end = pending.find_first_of ("\r\n"); end != std::string::npos; end = pending.find_first_of ("\r\n"))
			{
				const auto line = pending.substr (0, end);
				pending.erase (0, end + 1);
				if (! line.empty())
				{
					if (onLine)
						onLine (line);
					if (line.find ("%|") == std::string::npos)
						lastLine = line;
				}
			}
		}
		if (cancelRequested || threadShouldExit())
			::kill (-pid, SIGKILL);
	}
	close (out[0]);

	int statusCode = 0;
	waitpid (pid, &statusCode, 0);
	childGroup = -1;

	if (cancelRequested || threadShouldExit())
		return (error = "cancelled"), false;
	if (WIFEXITED (statusCode) && WEXITSTATUS (statusCode) == 0)
		return true;

	error = juce::String (argv[0] == "systemd-run" ? "demucs" : argv[0]) + ": "
		  + (lastLine.empty() ? juce::String ("failed") : juce::String (lastLine));
	return false;
}
