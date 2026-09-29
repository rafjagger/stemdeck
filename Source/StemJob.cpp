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

StemJobPlan planStemJob (const std::string& library, const std::string& folder, const std::string& track, const std::string& originalFileName,
						 const std::function<bool (const std::string&)>& exists)
{
	StemJobPlan plan;
	const auto relative = sanitiseFolder (folder);
	plan.folder = relative.empty() ? library : library + "/" + relative;

	plan.stemExtension = stemExtensionFor (originalFileName);
	const auto base = sanitiseName (track, "Untitled");
	plan.track = base;
	for (int n = 2; exists (plan.folder + "/" + stemFileName (plan.track, 0, plan.stemExtension)); ++n)
		plan.track = base + " (" + std::to_string (n) + ")";

	for (int stem = 0; stem < 4; ++stem)
		plan.stemPaths[(size_t) stem] = plan.folder + "/" + stemFileName (plan.track, stem, plan.stemExtension);
	plan.originalPath = plan.folder + "/originals/" + plan.track + extensionOf (originalFileName);
	return plan;
}

std::string stemExtensionFor (const std::string& originalFileName)
{
	auto extension = extensionOf (originalFileName);
	std::transform (extension.begin(), extension.end(), extension.begin(), [] (unsigned char c) { return (char) std::tolower (c); });

	for (const auto* kept : { ".flac", ".wav", ".aiff", ".aif", ".ogg" })
		if (extension == kept)
			return extension.substr (1);
	return "flac";
}

std::vector<std::string> encodeCommand (const std::string& stemWav, const std::string& output, const std::string& extension)
{
	std::vector<std::string> argv { "nice", "-n", "19", "ionice", "-c", "3",
									"ffmpeg", "-nostdin", "-hide_banner", "-loglevel", "error", "-y", "-i", stemWav };
	if (extension == "flac")
		argv.insert (argv.end(), { "-c:a", "flac", "-sample_fmt", "s32", "-bits_per_raw_sample", "24" });
	else if (extension == "aiff" || extension == "aif")
		argv.insert (argv.end(), { "-c:a", "pcm_s24be" });
	else if (extension == "ogg")
		argv.insert (argv.end(), { "-c:a", "libvorbis", "-q:a", "8" });
	argv.push_back (output);
	return argv;
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

int StemJobQueue::add (const std::string& input, const std::string& folder, const std::string& track)
{
	entries.push_back ({ nextId, input, folder, track, JobState::queued, {} });
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

bool isSeparableAudioFile (const std::string& fileName)
{
	auto name = fileName;
	std::transform (name.begin(), name.end(), name.begin(), [] (unsigned char c) { return (char) std::tolower (c); });

	static const char* const formats[] = { ".flac", ".wav", ".mp3", ".aiff", ".aif", ".ogg", ".m4a", ".opus" };
	const auto extension = extensionOf (name);
	if (std::find (std::begin (formats), std::end (formats), extension) == std::end (formats))
		return false;

	// What the stem creator writes: "- 1 - drums.flac", before that "- 1.drums.wav".
	for (int stem = 0; stem < 4; ++stem)
		for (const auto* between : { " - ", "." })
			if (name.find ("- " + std::to_string (stem + 1) + between + stemNames[stem] + ".") != std::string::npos)
				return false;
	return true;
}

ArtistAlbum guessArtistAlbum (const std::string& path)
{
	std::vector<std::string> folders;
	for (size_t start = 0, slash; (slash = path.find ('/', start)) != std::string::npos; start = slash + 1)
		if (slash > start)
			folders.push_back (path.substr (start, slash - start));

	ArtistAlbum guess;
	if (! folders.empty())
		guess.album = folders.back();
	if (folders.size() > 1)
		guess.artist = folders[folders.size() - 2];
	return guess;
}

bool shouldOfferForSeparation (const std::string& path, const std::string& libraryFolder)
{
	auto root = libraryFolder;
	while (root.size() > 1 && root.back() == '/')
		root.pop_back();
	const auto insideLibrary = path.size() > root.size() && path.compare (0, root.size(), root) == 0 && path[root.size()] == '/';
	return ! insideLibrary && isSeparableAudioFile (path.substr (path.rfind ('/') + 1));
}

std::string sanitiseFolder (const std::string& folder)
{
	std::string clean;
	for (size_t start = 0; start <= folder.size();)
	{
		auto end = folder.find_first_of ("/\\", start);
		if (end == std::string::npos)
			end = folder.size();
		const auto level = sanitiseName (folder.substr (start, end - start), "");
		if (! level.empty())
			clean += (clean.empty() ? "" : "/") + level;
		start = end + 1;
	}
	return clean;
}

std::string suggestTargetFolder (const std::vector<std::string>& paths)
{
	if (paths.empty())
		return {};

	// The folder every path lies in.
	const auto first = paths.front().substr (0, paths.front().rfind ('/') + 1);
	auto common = first;
	bool oneFolder = true;
	for (const auto& path : paths)
	{
		oneFolder = oneFolder && path.substr (0, path.rfind ('/') + 1) == first;
		while (! common.empty() && path.compare (0, common.size(), common) != 0)
			common = common.substr (0, common.rfind ('/', common.size() - 2) + 1);
	}

	const auto guess = guessArtistAlbum (common);
	if (! oneFolder || guess.artist.empty())
		return sanitiseFolder (guess.album);
	return sanitiseFolder (guess.artist + "/" + guess.album);
}
