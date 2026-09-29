#include "StemJob.h"

#include "StemNames.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace
{
	std::string extensionOf (const std::string& fileName)
	{
		const auto dot = fileName.rfind ('.');
		return dot == std::string::npos || dot == 0 ? std::string() : fileName.substr (dot);
	}

	std::string baseNameOf (const std::string& path)
	{
		const auto slash = path.rfind ('/');
		auto name = slash == std::string::npos ? path : path.substr (slash + 1);
		const auto dot = name.rfind ('.');
		return dot == std::string::npos ? name : name.substr (0, dot);
	}

	const char* const stemNames[] = { "drums", "bass", "other", "vocals" };
}

StemJobPlan planStemJob (const std::string& library, const std::string& artist, const std::string& album,
						 const std::string& track, const std::string& originalFileName,
						 const std::function<bool (const std::string&)>& exists)
{
	StemJobPlan plan;
	plan.albumFolder = library + "/" + sanitiseName (artist, "Unknown Artist") + "/" + sanitiseName (album, "Unknown Album");

	const auto base = sanitiseName (track, "Untitled");
	plan.track = base;
	for (int n = 2; exists (plan.albumFolder + "/" + stemFileName (plan.track, 0, "wav")); ++n)
		plan.track = base + " (" + std::to_string (n) + ")";

	for (int stem = 0; stem < 4; ++stem)
		plan.stemPaths[(size_t) stem] = plan.albumFolder + "/" + stemFileName (plan.track, stem, "wav");
	plan.originalPath = plan.albumFolder + "/originals/" + plan.track + extensionOf (originalFileName);
	return plan;
}

std::vector<std::string> decodeCommand (const std::string& input, const std::string& outputWav)
{
	return { "ffmpeg", "-nostdin", "-hide_banner", "-loglevel", "error", "-y",
			 "-i", input, "-vn", "-ac", "2", "-ar", "44100", "-c:a", "pcm_f32le", outputWav };
}

std::vector<std::string> separateCommand (const std::string& venv, const std::string& inputWav,
										  const std::string& stagingDir, bool fast)
{
	// A scope with a memory cap (the user manager may set it without sudo);
	// CPU 0 is the one core without real-time audio on the rig, and idle
	// scheduling lets even the UI renderer there go first.
	return { "systemd-run", "--user", "--scope", "--quiet", "-p", "MemoryMax=6G", "-p", "CPUWeight=idle",
			 "taskset", "-c", fast ? "0-3" : "0",
			 "chrt", "-i", "0",
			 "nice", "-n", "19",
			 "ionice", "-c", "3",
			 venv + "/bin/demucs", "-n", "htdemucs", "--int24", "--clip-mode", "clamp",
			 "-o", stagingDir, inputWav };
}

std::vector<std::string> separateEnvironment (bool fast)
{
	const std::string threads = fast ? "4" : "1";
	return { "OMP_NUM_THREADS=" + threads, "MKL_NUM_THREADS=" + threads };
}

std::string demucsOutputFile (const std::string& stagingDir, const std::string& inputWav, int stem)
{
	return stagingDir + "/htdemucs/" + baseNameOf (inputWav) + "/" + stemNames[stem] + ".wav";
}

std::optional<double> parseDemucsProgress (const std::string& line)
{
	// tqdm rewrites its bar with '\r'; the last one on the line is the latest.
	const auto start = line.rfind ('\r');
	const auto last = start == std::string::npos ? line : line.substr (start + 1);

	const auto bar = last.find ("%|");
	if (bar == std::string::npos)
		return std::nullopt;

	auto digits = bar;
	while (digits > 0 && std::isdigit ((unsigned char) last[digits - 1]))
		--digits;
	if (digits == bar)
		return std::nullopt;

	return std::stoi (last.substr (digits, bar - digits)) / 100.0;
}

int StemJobQueue::add (const std::string& input, const std::string& artist, const std::string& album, const std::string& track)
{
	entries.push_back ({ nextId, input, artist, album, track, JobState::queued, {} });
	return nextId++;
}

std::optional<int> StemJobQueue::startNext()
{
	if (running())
		return std::nullopt;

	for (auto& e : entries)
		if (e.state == JobState::queued)
		{
			e.state = JobState::running;
			return e.id;
		}
	return std::nullopt;
}

void StemJobQueue::finished (int id, bool ok, const std::string& message)
{
	for (auto& e : entries)
		if (e.id == id && e.state == JobState::running)
		{
			e.state = ok ? JobState::done : JobState::failed;
			e.message = message;
		}
}

StemJobQueue::Cancel StemJobQueue::cancel (int id)
{
	const auto at = std::find_if (entries.begin(), entries.end(), [id] (const StemJobEntry& e) { return e.id == id; });
	if (at == entries.end())
		return Cancel::none;

	if (at->state == JobState::queued)
	{
		entries.erase (at);
		return Cancel::removed;
	}
	if (at->state == JobState::running)
	{
		at->state = JobState::cancelled;
		return Cancel::kill;
	}
	return Cancel::none;
}

std::optional<int> StemJobQueue::running() const
{
	for (const auto& e : entries)
		if (e.state == JobState::running)
			return e.id;
	return std::nullopt;
}

const StemJobEntry* StemJobQueue::find (int id) const
{
	for (const auto& e : entries)
		if (e.id == id)
			return &e;
	return nullptr;
}
