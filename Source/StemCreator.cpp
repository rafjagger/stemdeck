#include "StemCreator.h"

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
	processes.kill();   // nothing of a job outlives StemDeck; SIGKILL reaches stopped processes too
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

juce::File StemCreator::venv() const
{
	std::lock_guard<std::mutex> guard (lock);
	return venvFolder;
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
		processes.kill();
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
	paused = shouldPause;
	processes.setPaused (shouldPause);
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

void StemCreator::run()
{
	while (! threadShouldExit())
	{
		std::optional<StemJobEntry> job;
		{
			std::lock_guard<std::mutex> guard (lock);
			if (const auto id = queue.startNext())
			{
				cancelRequested = false;
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
							 .getChildFile (".cache/StemDeck/jobs/" + juce::String ((int) getpid()) + "-" + juce::String (job.id));
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

	// Into the original's format, still in staging: a cancel here leaves the
	// library untouched.
	std::array<juce::File, 4> stems;
	for (int stem = 0; stem < 4; ++stem)
	{
		const auto separated = demucsOutputFile (staging.getFullPathName().toStdString(), wav, stem);
		stems[(size_t) stem] = juce::File (separated);
		if (plan.stemExtension == "wav")
			continue;

		stems[(size_t) stem] = staging.getChildFile ("stem-" + juce::String (stem + 1) + "." + juce::String (plan.stemExtension));
		if (! execute (encodeCommand (separated, stems[(size_t) stem].getFullPathName().toStdString(), plan.stemExtension), {}, {}, error))
			return cleanUp(), false;
	}

	// The original first: the copy is the slow part (a stick, a big FLAC), and
	// a cancel during it must still leave the album untouched.
	const juce::File original (plan.originalPath);
	original.getParentDirectory().createDirectory();
	if (! input.copyFileTo (original))
		return (error = "could not copy the original"), cleanUp(), false;

	if (stopRequested())
	{
		original.deleteFile();
		return (error = "cancelled"), cleanUp(), false;
	}

	// From here on the set goes in: four renames, too quick to cancel.
	for (int stem = 0; stem < 4; ++stem)
	{
		const auto& from = stems[(size_t) stem];
		if (! from.moveFileTo (juce::File (plan.stemPaths[(size_t) stem])))
		{
			for (int s = 0; s < stem; ++s)
				juce::File (plan.stemPaths[(size_t) s]).deleteFile();  // no three-stem set
			original.deleteFile();
			error = "could not move " + from.getFileName();
			return cleanUp(), false;
		}
	}
	cleanUp();

	const juce::File firstStem (plan.stemPaths[0]);
	juce::MessageManager::callAsync ([this, firstStem] { if (onSetCreated) onSetCreated (firstStem); });
	return true;
}

bool StemCreator::execute (const std::vector<std::string>& argv, const std::vector<std::string>& environment,
						   const std::function<void (const std::string&)>& onLine, juce::String& error)
{
	std::string lastLine;
	const auto result = processes.run (argv, environment, onLine, [this] { return stopRequested(); }, lastLine);
	if (result == ProcessGroup::Result::ok)
		return true;
	if (result == ProcessGroup::Result::stopped)
		return (error = "cancelled"), false;

	const auto program = argv[0] == "systemd-run" ? "demucs" : argv[0] == "nice" ? "ffmpeg" : argv[0];
	error = juce::String (program) + ": "
		  + (lastLine.empty() ? juce::String ("failed") : juce::String (lastLine));
	return false;
}
