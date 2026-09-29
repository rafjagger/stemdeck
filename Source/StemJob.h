#pragma once

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <vector>

// The stem creator's rules, without JUCE and without the model: where a job's
// files go, the command lines it runs, the progress it reports and the queue
// it waits in. StemCreator (JUCE) runs what these say.

// Where one separated track lands:
//   <library>/<Artist>/<Album>/<Track> - 1.drums.wav .. 4.vocals.wav
//   <library>/<Artist>/<Album>/originals/<Track>.<original's extension>
struct StemJobPlan
{
	std::string albumFolder;
	std::string track;                    // after sanitising and "(2)" if taken
	std::array<std::string, 4> stemPaths; // bus order: drums, bass, other, vocals
	std::string originalPath;
};

// `exists` answers whether a path is already taken (the library as it is).
StemJobPlan planStemJob (const std::string& library, const std::string& artist, const std::string& album,
						 const std::string& track, const std::string& originalFileName,
						 const std::function<bool (const std::string&)>& exists);

// ffmpeg: any input to the 44.1 kHz stereo float WAV the engine reads.
std::vector<std::string> decodeCommand (const std::string& input, const std::string& outputWav);

// Demucs htdemucs at idle priority on CPU 0 with one thread (Fast: all cores),
// memory capped, 24-bit clamped output into `stagingDir`.
std::vector<std::string> separateCommand (const std::string& venv, const std::string& inputWav,
										  const std::string& stagingDir, bool fast);
std::vector<std::string> separateEnvironment (bool fast);

// Where Demucs writes stem `stem` (0 drums .. 3 vocals) of `inputWav`.
std::string demucsOutputFile (const std::string& stagingDir, const std::string& inputWav, int stem);

// Demucs' tqdm progress on stderr, e.g. " 43%|####      | 12.3/28.6 [...]",
// as 0..1; nothing for any other line.
std::optional<double> parseDemucsProgress (const std::string& line);

// One job at a time, in the order added; a failure does not stop the next.
enum class JobState { queued, running, done, failed, cancelled };

struct StemJobEntry
{
	int id = 0;
	std::string input, artist, album, track;
	JobState state = JobState::queued;
	std::string message;
};

class StemJobQueue
{
public:
	int add (const std::string& input, const std::string& artist, const std::string& album, const std::string& track);
	std::optional<int> startNext();                   // nothing while one runs
	void finished (int id, bool ok, const std::string& message);

	enum class Cancel { removed, kill, none };
	Cancel cancel (int id);                           // kill: the running one, once

	std::optional<int> running() const;
	const std::vector<StemJobEntry>& jobs() const { return entries; }
	const StemJobEntry* find (int id) const;

private:
	std::vector<StemJobEntry> entries;
	int nextId = 1;
};
