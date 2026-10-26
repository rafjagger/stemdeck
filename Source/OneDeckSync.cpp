// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneDeckSync.h"

#include "DeckFollow.h"
#include "DeckPanel.h"
#include "StemDeckPlayer.h"

OneDeckSync::OneDeckSync (StemDeckPlayer& p, DeckPanel& d) : player (p), panel (d) {}

void OneDeckSync::setSynced (bool on)
{
	synced = on;
	multiple = 0.0;   // chosen on the first update
	deckfollow::setBent (panel, player, bent, false);
	if (! on)
		player.setSyncNudge (1.0);
}

void OneDeckSync::pressMaster (double seconds)
{
	master.press (seconds);
	panel.setMaster (master.isMaster());
	if (master.isMaster())
		player.setSyncNudge (1.0);   // the master leads: no correction of its own
}

void OneDeckSync::joined (double seconds)
{
	master.joined (seconds);
}

void OneDeckSync::hear (const std::vector<ProLinkReceiver::Event>& events)
{
	for (const auto& event : events)
	{
		if (const auto* beat = std::get_if<prolink::BeatPacket> (&event.packet))
			linkClock.onBeat (*beat, event.seconds);
		else if (const auto* status = std::get_if<prolink::StatusPacket> (&event.packet))
		{
			linkClock.onStatus (*status, event.seconds);
			master.onStatus (status->device, status->isMaster, event.seconds);
		}
	}
}

bool OneDeckSync::update (double seconds)
{
	const auto yielded = master.yieldIfOvertaken (seconds);
	if (yielded)
		panel.setMaster (false);

	if (! synced || isMaster() || linkClock.bpm() <= 0.0 || ! player.getBeatGrid().isValid())
		return yielded;   // nothing to follow: the deck's own grid

	auto in = leaderInputAt (linkClock, seconds);
	in.anyScratching = player.isScratching();
	in.multiple = multiple;
	// The link's phase is for now; the deck's position is from its last audio
	// block -- carried forward, as the two-deck app does under SYNC: LINK.
	const auto position = positionAt (player.getPosition(), player.getPositionStamp(), seconds,
									  player.getEffectiveRate(), player.isPlaying() && ! player.isScratching());
	multiple = deckfollow::apply (panel, player, bent, in, position);
	return yielded;
}
