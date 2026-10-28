// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "StemDeckPlayer.h"
#include "TransportDeck.h"

// The player as the transport keys see it (TransportDeck.h).
class PlayerTransport : public TransportDeck
{
public:
	explicit PlayerTransport (StemDeckPlayer& p) : player (p) {}

	bool isLoaded() const override { return player.isLoaded(); }
	bool isPlaying() const override { return player.isPlaying(); }
	void play() override { player.play(); }
	void pause() override { player.pause(); }

	double position() const override { return player.getPosition(); }
	void setPosition (double seconds) override { player.setPosition (seconds); }
	double cuePoint() const override { return player.getCuePoint(); }
	void setCuePoint (double seconds) override { player.setCuePoint (seconds); }

	bool hasLoop() const override { return player.hasLoop(); }
	double loopStart() const override { return player.getLoop().getStart(); }
	double loopEnd() const override { return player.getLoop().getEnd(); }
	void setLoop (double startSeconds, double endSeconds) override { player.setLoop (startSeconds, endSeconds); }
	void clearLoop() override { player.clearLoop(); }

private:
	StemDeckPlayer& player;
};
