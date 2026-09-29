#pragma once

#include <JuceHeader.h>
#include "StemSet.h"
#include "TempoAnalysis.h"

// Plays the four stems of a StemSet from one shared playhead, so they stay
// sample-locked. Output is 8 channels: stem N -> channels 2N, 2N+1.
//
// WAV/AIFF stems are memory-mapped and read directly in the audio callback,
// which makes seeking and looping instant; a background thread touches the
// pages ahead of the playhead so the callback does not wait on the disk.
// Other formats fall back to a buffered reader.
class StemDeckPlayer : public juce::AudioSource,
					   private juce::TimeSliceClient
{
public:
	static constexpr int numStems = StemSet::numStems;
	static constexpr int numOutputChannels = numStems * 2;

	explicit StemDeckPlayer (juce::AudioFormatManager& formatManager);
	~StemDeckPlayer() override;

	// Returns an error message, or an empty string on success.
	juce::String load (const StemSet& set);
	bool isLoaded() const { return lengthInSamples.load() > 0; }

	void play();
	void pause();
	void stop(); // pause and return to the start (or the loop start)
	bool isPlaying() const { return playing.load(); }

	// At the end of the track, start again from the beginning instead of stopping.
	void setRepeat (bool shouldRepeat) { repeat = shouldRepeat; }
	bool isRepeating() const { return repeat.load(); }

	void setPosition (double seconds);
	double getPosition() const;
	// When the audio thread last moved the position, in
	// juce::Time::getMillisecondCounterHiRes() seconds -- see positionAt().
	double getPositionStamp() const { return positionStamp.load(); }
	double getLength() const;

	// Loop between two positions in seconds; the playhead jumps to the start.
	void setLoop (double startSeconds, double endSeconds);
	void clearLoop();
	bool hasLoop() const { return loopEnd.load() > 0; }
	juce::Range<double> getLoop() const;

	void setSpeed (double ratio);            // tempo fader
	double getSpeed() const { return speed.load(); }
	void setPitchBend (double factor) { pitchBend = factor; } // jog ring, temporary
	void setSyncNudge (double factor) { syncNudge = factor; } // phase correction while synced
	double getEffectiveRate() const { return speed.load() * pitchBend.load() * syncNudge.load(); }

	// Scratching (jog wheel top in vinyl mode): while active the playhead
	// follows a target position that scratchBy() moves, forwards or backwards,
	// regardless of play/pause. Afterwards the deck carries on as it was.
	void beginScratch();
	void scratchBy (double seconds);
	void endScratch() { scratching = false; }
	bool isScratching() const { return scratching.load(); }

	void setBeatGrid (const BeatGrid& grid) { gridBpm = grid.bpm; gridFirstBeat = grid.firstBeat; }
	BeatGrid getBeatGrid() const { return { gridBpm.load(), gridFirstBeat.load() }; }
	void setStemGain (int stem, float gain);
	void setStemMuted (int stem, bool muted);
	bool isStemMuted (int stem) const { return stemMuted[(size_t) stem].load(); }

	// Routing only, applied by the mixer (Buses.h): which buses a stem is on,
	// any number of them, and whether the whole deck goes to PHONES.
	void setStemOnBus (int stem, int bus, bool on)
	{
		const auto bit = 1u << bus;
		if (on) stemBuses[(size_t) stem] |= bit;
		else    stemBuses[(size_t) stem] &= ~bit;
	}
	bool isStemOnBus (int stem, int bus) const { return (stemBuses[(size_t) stem].load() >> bus) & 1u; }
	void setDeckPhones (bool on) { deckPhones = on; }
	bool isDeckPhones() const { return deckPhones.load(); }

	// Channel fader. Not applied here: the mixer puts it on the program buses
	// and leaves PHONES pre fader. The stem output is after knob and mute.
	void setDeckGain (float gain) { deckGain = gain; }
	float getDeckGain() const { return deckGain.load(); }

	// Only stored here so every view can draw it; reset to 0 on load.
	void setCuePoint (double seconds) { cuePoint = seconds; }
	double getCuePoint() const { return cuePoint.load(); }

	// Peak level after knob, mute and fader since the last call, for metering.
	float popStemPeak (int stem);

	// AudioSource: the buffer must have at least numOutputChannels channels.
	void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
	void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
	void releaseResources() override;

private:
	struct LoadedSet
	{
		std::array<std::unique_ptr<juce::AudioFormatReader>, numStems> readers;
		std::array<juce::MemoryMappedAudioFormatReader*, numStems> mapped {}; // non-owning, may be null
		juce::int64 length = 0;
		double sampleRate = 44100.0;
	};

	// Reads the stems at the playhead; this is what the resampler pulls from.
	struct StemReader : public juce::AudioSource
	{
		explicit StemReader (StemDeckPlayer& o) : owner (o) {}
		void prepareToPlay (int, double) override {}
		void releaseResources() override {}
		void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override { owner.readStems (info); }
		StemDeckPlayer& owner;
	};

	void readStems (const juce::AudioSourceChannelInfo& info);
	void renderScratch (const juce::AudioSourceChannelInfo& info);
	void updateResamplingRatio();
	int useTimeSlice() override;

	juce::AudioFormatManager& formatManager;
	juce::TimeSliceThread diskThread { "Stem prefetch" };
	juce::TimeSliceThread bufferThread { "Stem buffering" }; // for non-mappable formats

	// Held by the audio callback for its whole duration and by load() only for
	// the pointer swap. The prefetch thread takes a shared copy, so disk I/O
	// never happens while the lock is held.
	juce::CriticalSection setLock;
	std::shared_ptr<LoadedSet> loaded;
	std::atomic<juce::int64> lengthInSamples { 0 };

	std::atomic<bool> playing { false }, repeat { false };
	std::atomic<juce::int64> readPosition { 0 };
	std::atomic<double> positionStamp { 0.0 };
	std::atomic<juce::int64> loopStart { 0 }, loopEnd { 0 }; // loopEnd == 0: no loop
	std::atomic<double> fileSampleRate { 44100.0 }, deviceSampleRate { 44100.0 }, speed { 1.0 };
	std::atomic<double> pitchBend { 1.0 }, syncNudge { 1.0 };
	std::atomic<double> gridBpm { 0.0 }, gridFirstBeat { 0.0 };

	std::atomic<bool> scratching { false };
	std::atomic<double> scratchTarget { 0.0 };        // file samples; written by the UI only
	double scratchPosition = 0.0, scratchVelocity = 0.0; // audio thread only
	float scratchGain = 0.0f;
	bool wasScratching = false;
	juce::AudioBuffer<float> scratchBuffer;
	static constexpr double maxScratchSpeed = 16.0;

	std::atomic<float> deckGain { 1.0f };
	std::atomic<double> cuePoint { 0.0 };
	std::array<std::atomic<float>, numStems> stemGain;
	std::array<std::atomic<bool>, numStems> stemMuted;
	std::array<std::atomic<unsigned>, numStems> stemBuses;
	std::atomic<bool> deckPhones { false };
	std::array<std::atomic<float>, numStems> stemPeak;
	std::array<juce::SmoothedValue<float>, numStems> gainSmoothers;

	StemReader stemReader { *this };
	juce::ResamplingAudioSource resampler { &stemReader, false, numOutputChannels };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StemDeckPlayer)
};
