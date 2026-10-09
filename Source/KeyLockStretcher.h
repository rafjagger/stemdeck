#pragma once

#include "KeyLock.h"

#include <rubberband/RubberBandStretcher.h>

#include <cstdint>
#include <vector>

// Key lock for one stem: a Rubber Band stretcher, its reader and the
// bookkeeping of where the ear is. A deck runs one per stem: R2 decides its
// phase resets for all channels of a stretcher together, so in one shared
// stretcher the tonal stems talked the drums out of their transients (a
// click moved by up to 4 ms); apart, each stem keeps its own. Runs at the
// file's rate; the deck's resampler converts to the device's. No JUCE, so
// it can be tested offline.
//
// Built off the audio thread; restart() and render() allocate nothing.
class KeyLockStretcher
{
public:
	// The track as the stretcher reads it, from the audio thread.
	struct Input
	{
		virtual ~Input() = default;
		// Up to `count` frames from `position` on into `dest` (one pointer
		// per channel). Stops early at a loop's end or the track's end and
		// says how many it read.
		virtual int read (float* const* dest, int count, std::int64_t position) = 0;
		// Where the track goes on where `read` stopped: the loop's start,
		// the track's start (repeat), or -1: it ends.
		virtual std::int64_t jumpFrom (std::int64_t position) = 0;
	};

	// Builds and measures its alignment: allocates, a few milliseconds of
	// work -- never on the audio thread.
	KeyLockStretcher (int numChannels, double fileRate);
	// With an alignment measured before, for the same sample rate.
	KeyLockStretcher (int numChannels, double fileRate, const keylock::Alignment& known);

	// How the output lines up at `sampleRate`: measured on a stretcher of
	// this kind, since it depends on the engine's window and hop sizes,
	// which depend on the rate and may change between Rubber Band releases.
	static keylock::Alignment measureAlignment (double sampleRate);

	// Start at `position`: the stretcher is filled with the audio before
	// it, so the first block already plays from there.
	void restart (std::int64_t position, double playbackRate, Input& input);

	// `numSamples` frames at `playbackRate` into `out`.
	void render (float* const* out, int numSamples, double playbackRate, Input& input);

	// The playhead: where the output is in the track, latency taken off.
	double position() const { return map.position() - keylock::lagCorrection (alignment, rate, startRate); }
	// The reader is past the track's end; what comes now is silence.
	bool hasEnded() const { return ended; }

	double getSampleRate() const { return sampleRate; }
	int getNumChannels() const { return numChannels; }
	const keylock::Alignment& getAlignment() const { return alignment; }

private:

	void feed (Input& input);
	void fill (Input& input, int count);
	void process (int count);
	void setRate (double newRate);
	void discardStartDelay (Input& input);

	static constexpr int chunk = 1024;   // frames per process() call at most

	const int numChannels;
	const double sampleRate;
	RubberBand::RubberBandStretcher stretcher;
	std::vector<std::vector<float>> inputStore, scratchStore;
	std::vector<float*> inputs, scratch, offsetPointers;

	keylock::PositionMap map;
	keylock::OutputRuns runs;
	keylock::Alignment alignment;
	double rate = 1.0, startRate = 1.0;
	std::int64_t cursor = 0;   // where the reader goes on
	bool ended = false;
	std::size_t toDiscard = 0;
};
