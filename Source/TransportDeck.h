// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What the transport keys (CUE, PLAY, LOOP, LOOP IN, LOOP OUT) read from a
// deck and do to it, in seconds. Pure: no JUCE, so the keys' rules
// (CuePlay.h, DeckLoop.h) are tested on a plain fake; DeckPanel hands them
// its StemDeckPlayer.
class TransportDeck
{
public:
	virtual ~TransportDeck() = default;

	virtual bool isLoaded() const = 0;
	virtual bool isPlaying() const = 0;
	virtual void play() = 0;
	virtual void pause() = 0;

	virtual double position() const = 0;
	virtual void setPosition (double seconds) = 0;
	virtual double cuePoint() const = 0;
	virtual void setCuePoint (double seconds) = 0;

	// The deck puts a loop on its beat grid when it has one (the quantize)
	// and jumps the playhead to the loop's start.
	virtual bool hasLoop() const = 0;
	virtual double loopStart() const = 0;
	virtual double loopEnd() const = 0;
	virtual void setLoop (double startSeconds, double endSeconds) = 0;
	virtual void clearLoop() = 0;
};
