#include <gtest/gtest.h>

#include "StemJob.h"

#include <algorithm>
#include <set>

namespace
{
	bool contains (const std::vector<std::string>& argv, const std::string& word)
	{
		return std::find (argv.begin(), argv.end(), word) != argv.end();
	}

	bool inOrder (const std::vector<std::string>& argv, std::initializer_list<std::string> words)
	{
		auto at = argv.begin();
		for (const auto& w : words)
		{
			at = std::find (at, argv.end(), w);
			if (at == argv.end())
				return false;
			++at;
		}
		return true;
	}

	const auto nothingExists = [] (const std::string&) { return false; };
}

// ── The layout ──────────────────────────────────────────────────────────

TEST (StemJob, AJobLandsInItsTargetFolder)
{
	const auto plan = planStemJob ("/lib", "Burial/Untrue", "Archangel", "Archangel.flac", nothingExists);
	EXPECT_EQ (plan.folder, "/lib/Burial/Untrue");
	EXPECT_EQ (plan.track, "Archangel");
	EXPECT_EQ (plan.stemPaths[0], "/lib/Burial/Untrue/Archangel - 1 - drums.flac");
	EXPECT_EQ (plan.stemPaths[3], "/lib/Burial/Untrue/Archangel - 4 - vocals.flac");
	EXPECT_EQ (plan.stemExtension, "flac");
	EXPECT_EQ (plan.originalPath, "/lib/Burial/Untrue/originals/Archangel.flac");
}

TEST (StemJob, ATakenTrackNameGetsANumber)
{
	const std::set<std::string> taken { "/lib/A/B/Intro - 1 - drums.flac", "/lib/A/B/Intro (2) - 1 - drums.flac" };
	const auto plan = planStemJob ("/lib", "A/B", "Intro", "intro.mp3",
								   [&taken] (const std::string& p) { return taken.count (p) > 0; });
	EXPECT_EQ (plan.track, "Intro (3)");
	EXPECT_EQ (plan.originalPath, "/lib/A/B/originals/Intro (3).mp3");
}

TEST (StemJob, NoTargetFolderIsTheLibraryItself)
{
	const auto plan = planStemJob ("/lib", " ", "Track/1", "t.wav", nothingExists);
	EXPECT_EQ (plan.folder, "/lib");
	EXPECT_EQ (plan.track, "Track_1");
	EXPECT_EQ (plan.originalPath, "/lib/originals/Track_1.wav");
}

TEST (StemJob, ATypedFolderStaysInsideTheLibrary)
{
	EXPECT_EQ (sanitiseFolder ("Burial/Untrue"), "Burial/Untrue");
	EXPECT_EQ (sanitiseFolder ("/Burial//Untrue/"), "Burial/Untrue");
	EXPECT_EQ (sanitiseFolder ("../../etc"), "etc");
	EXPECT_EQ (sanitiseFolder ("A\\B"), "A/B");
	EXPECT_EQ (sanitiseFolder ("  "), "");
	EXPECT_EQ (planStemJob ("/lib", "../x", "t", "t.flac", nothingExists).folder, "/lib/x");
}

TEST (StemJob, TheSuggestedFolderIsArtistAlbumOfOneFolder)
{
	EXPECT_EQ (suggestTargetFolder ({ "/music/Burial/Untrue/1.flac", "/music/Burial/Untrue/2.flac" }), "Burial/Untrue");
	EXPECT_EQ (suggestTargetFolder ({ "/music/Burial/Untrue/1.flac", "/music/Burial/Rival/2.flac" }), "Burial")
		<< "several albums: only the artist they share";
	EXPECT_EQ (suggestTargetFolder ({ "/a/1.flac", "/b/2.flac" }), "");
	EXPECT_EQ (suggestTargetFolder ({ "/Untrue/1.flac" }), "Untrue");
	EXPECT_EQ (suggestTargetFolder ({}), "");
}

TEST (StemJob, StemsKeepTheOriginalsFormatWhereItPlays)
{
	EXPECT_EQ (stemExtensionFor ("a.flac"), "flac");
	EXPECT_EQ (stemExtensionFor ("a.WAV"), "wav");
	EXPECT_EQ (stemExtensionFor ("a.aiff"), "aiff");
	EXPECT_EQ (stemExtensionFor ("a.ogg"), "ogg");
	EXPECT_EQ (stemExtensionFor ("a.mp3"), "flac") << "JUCE here reads no MP3";
	EXPECT_EQ (stemExtensionFor ("a.m4a"), "flac");
	EXPECT_EQ (stemExtensionFor ("a.opus"), "flac");
	EXPECT_EQ (planStemJob ("/lib", "A/B", "t", "t.wav", nothingExists).stemPaths[1], "/lib/A/B/t - 2 - bass.wav");
}

// ── The command lines ───────────────────────────────────────────────────

TEST (StemJob, StemsAreEncodedAt24Bit)
{
	const auto flac = encodeCommand ("/stage/drums.wav", "/stage/stem-1.flac", "flac");
	EXPECT_TRUE (inOrder (flac, { "nice", "ionice", "ffmpeg", "-i", "/stage/drums.wav", "flac", "24", "/stage/stem-1.flac" }));
	EXPECT_TRUE (contains (encodeCommand ("/s/d.wav", "/s/1.aiff", "aiff"), "pcm_s24be"));
	EXPECT_TRUE (contains (encodeCommand ("/s/d.wav", "/s/1.ogg", "ogg"), "libvorbis"));
}

TEST (StemJob, AnyFormatIsDecodedToOneWav)
{
	const auto argv = decodeCommand ("/in/Song.m4a", "/stage/input.wav");
	EXPECT_EQ (argv.front(), "ffmpeg");
	EXPECT_TRUE (inOrder (argv, { "-i", "/in/Song.m4a", "-ac", "2", "-ar", "44100", "/stage/input.wav" }));
	EXPECT_TRUE (contains (argv, "-nostdin"));
	EXPECT_TRUE (contains (argv, "pcm_f32le"));
}

TEST (StemJob, SeparationRunsIdleOnCpuZeroWithOneThread)
{
	const auto argv = separateCommand ("/venv", "/stage/input.wav", "/stage", false);
	EXPECT_TRUE (inOrder (argv, { "systemd-run", "--user", "--scope", "MemoryMax=6G", "taskset", "-c", "0",
								  "chrt", "-i", "0", "nice", "-n", "19", "ionice", "-c", "3", "/venv/bin/demucs" }));
	EXPECT_TRUE (inOrder (argv, { "-n", "htdemucs", "--int24", "--clip-mode", "clamp", "-o", "/stage", "/stage/input.wav" }));
	EXPECT_TRUE (contains (separateEnvironment (false), "OMP_NUM_THREADS=1"));
}

TEST (StemJob, FastModeUsesAllCoresStillIdle)
{
	const auto argv = separateCommand ("/venv", "/stage/input.wav", "/stage", true);
	EXPECT_TRUE (inOrder (argv, { "taskset", "-c", "0-3", "chrt", "-i", "0" }));
	EXPECT_TRUE (contains (separateEnvironment (true), "OMP_NUM_THREADS=4"));
}

TEST (StemJob, DemucsWritesIntoItsModelFolder)
{
	EXPECT_EQ (demucsOutputFile ("/stage", "/stage/input.wav", 0), "/stage/htdemucs/input/drums.wav");
	EXPECT_EQ (demucsOutputFile ("/stage", "/stage/input.wav", 3), "/stage/htdemucs/input/vocals.wav");
}

// ── Progress ────────────────────────────────────────────────────────────

TEST (StemJob, TheProgressBarIsRead)
{
	EXPECT_NEAR (*parseDemucsProgress (" 43%|####      | 12.3/28.6 [00:10<00:12,  1.2seconds/s]"), 0.43, 1e-9);
	EXPECT_NEAR (*parseDemucsProgress ("\r  5%|          | 1.0/28.6\r100%|##########| 28.6/28.6 [00:20<00:00]"), 1.0, 1e-9)
		<< "the last update on a line counts";
}

TEST (StemJob, OtherLinesAreNoProgress)
{
	EXPECT_FALSE (parseDemucsProgress ("Separated tracks will be stored in /stage/htdemucs").has_value());
	EXPECT_FALSE (parseDemucsProgress ("").has_value());
	EXPECT_FALSE (parseDemucsProgress ("100 percent sure|not a bar").has_value());
}

// ── The queue ───────────────────────────────────────────────────────────

TEST (StemJob, JobsRunOneAtATimeInOrder)
{
	StemJobQueue q;
	const auto a = q.add ("a.flac", "A/B", "a");
	const auto b = q.add ("b.flac", "A/B", "b");
	EXPECT_EQ (q.startNext(), a);
	EXPECT_FALSE (q.startNext().has_value()) << "one at a time";
	q.finished (a, true, "");
	EXPECT_EQ (q.find (a)->state, JobState::done);
	EXPECT_EQ (q.startNext(), b);
}

TEST (StemJob, AFailureDoesNotStopTheNext)
{
	StemJobQueue q;
	const auto a = q.add ("a.xyz", "A/B", "a");
	const auto b = q.add ("b.flac", "A/B", "b");
	q.startNext();
	q.finished (a, false, "ffmpeg: Invalid data found when processing input");
	EXPECT_EQ (q.find (a)->state, JobState::failed);
	EXPECT_EQ (q.find (a)->message, "ffmpeg: Invalid data found when processing input");
	EXPECT_EQ (q.startNext(), b);
}

TEST (StemJob, CancellingAWaitingJobRemovesIt)
{
	StemJobQueue q;
	q.add ("a.flac", "A/B", "a");
	const auto b = q.add ("b.flac", "A/B", "b");
	q.startNext();
	EXPECT_EQ (q.cancel (b), StemJobQueue::Cancel::removed);
	EXPECT_EQ (q.find (b), nullptr);
}

TEST (StemJob, CancellingTheRunningJobKillsItOnce)
{
	StemJobQueue q;
	const auto a = q.add ("a.flac", "A/B", "a");
	q.startNext();
	EXPECT_EQ (q.cancel (a), StemJobQueue::Cancel::kill);
	EXPECT_EQ (q.find (a)->state, JobState::cancelled);
	EXPECT_EQ (q.cancel (a), StemJobQueue::Cancel::none);
	EXPECT_FALSE (q.running().has_value());
}

TEST (StemJob, TheTypicalFormatsAreAccepted)
{
	for (const auto* name : { "a.flac", "a.wav", "a.mp3", "a.aiff", "a.aif", "a.ogg", "a.m4a", "a.opus", "A.FLAC" })
		EXPECT_TRUE (isSeparableAudioFile (name)) << name;
}

TEST (StemJob, OtherFilesAndStemsAreNot)
{
	for (const auto* name : { "cover.jpg", "notes.txt", "flac", "Title - 1.drums.wav", "Title - 4.vocals.flac",
							   "Title - 1 - drums.flac", "A - B - 3 - other.ogg" })
		EXPECT_FALSE (isSeparableAudioFile (name)) << name;
}

TEST (StemJob, ArtistAndAlbumComeFromTheFolders)
{
	const auto guess = guessArtistAlbum ("/music/Burial/Untrue/02 Archangel.flac");
	EXPECT_EQ (guess.artist, "Burial");
	EXPECT_EQ (guess.album, "Untrue");
}

TEST (StemJob, AShallowPathGuessesWhatItHas)
{
	EXPECT_EQ (guessArtistAlbum ("/track.flac").artist, "");
	EXPECT_EQ (guessArtistAlbum ("/track.flac").album, "");
	EXPECT_EQ (guessArtistAlbum ("/Untrue/track.flac").album, "Untrue");
	EXPECT_EQ (guessArtistAlbum ("/Untrue/track.flac").artist, "");
}

TEST (StemJob, FilesInTheLibraryAreNotOfferedAgain)
{
	EXPECT_FALSE (shouldOfferForSeparation ("/lib/Burial/Untrue/originals/Archangel.flac", "/lib"));
	EXPECT_FALSE (shouldOfferForSeparation ("/lib/Burial/Untrue/Archangel-001.wav", "/lib"));
	EXPECT_FALSE (shouldOfferForSeparation ("/lib/Burial/Untrue/cover.jpg", "/lib"));
}

TEST (StemJob, FilesOutsideTheLibraryAreOffered)
{
	EXPECT_TRUE (shouldOfferForSeparation ("/music/Burial/Untrue/Archangel.flac", "/lib"));
	EXPECT_TRUE (shouldOfferForSeparation ("/library2/Archangel.flac", "/lib"));   // a sibling, not inside
	EXPECT_FALSE (shouldOfferForSeparation ("/music/notes.txt", "/lib"));
}
