// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ProLinkTempoMaster.h"

#include <algorithm>
#include <utility>

namespace prolink
{
	namespace
	{
		constexpr int noHandoff = 0xff;
	}

	TempoMaster::TempoMaster (int device, std::string deviceName, double started)
		: self (device), name (std::move (deviceName)), startedAt (started)
	{
	}

	void TempoMaster::setLocal (bool track, bool isPlaying, bool isSynced)
	{
		hasTrack = track;
		playing = isPlaying;
		synced = isSynced;
	}

	void TempoMaster::want (bool on, double)
	{
		asked = on;
		if (on)
			return;
		commanded = false;
		requestedFrom = 0;
		if (master)
			giveUp();
	}

	MasterState TempoMaster::state() const
	{
		return { master, handoffTo, syncCounter };
	}

	TempoMaster::Change TempoMaster::takeChange()
	{
		return std::exchange (change, Change::none);
	}

	const TempoMaster::Peer* TempoMaster::currentMaster (double now) const
	{
		for (const auto& [device, peer] : peers)
			if (peer.status.isMaster && isFresh (peer, now))
				return &peer;
		return nullptr;
	}

	void TempoMaster::becomeMaster (double now)
	{
		if (! asked)
			change = Change::granted;
		master = true;
		masterSince = now;
		handoffTo = noHandoff;
		requestedFrom = 0;
		commanded = false;
	}

	// sync.html: the outgoing master's Syncn goes one above any other player's.
	void TempoMaster::giveUp()
	{
		master = false;
		handoffTo = noHandoff;
		syncCounter = highestSyncCounter + 1;
		requestedFrom = 0;
	}

	void TempoMaster::yield()
	{
		giveUp();
		asked = false;
		commanded = false;
		change = Change::yielded;
	}

	std::vector<TempoMaster::Send> TempoMaster::hear (const StatusPacket& status, double now)
	{
		if (status.device == self)
			return {};

		auto& peer = peers[status.device];
		const auto wasClaiming = peer.claimSince >= 0.0 && isFresh (peer, now);
		peer.status = status;
		peer.last = now;
		peer.claimSince = ! status.isMaster ? -1.0 : wasClaiming ? peer.claimSince : now;
		highestSyncCounter = std::max (highestSyncCounter, status.syncCounter);

		if (! status.isMaster)
			return {};

		if (status.handoffTo == self)
		{
			if (! master)
				becomeMaster (now);
			return {};
		}

		// Another master asserting itself: the one we handed to, or a newer
		// claim made without the handoff. An older one is still letting go.
		if (master && status.handoffTo == noHandoff
			&& (handoffTo == status.device || peer.claimSince > masterSince))
			yield();
		return {};
	}

	std::vector<TempoMaster::Send> TempoMaster::hear (const Control& control, double now)
	{
		using Type = Control::Type;
		switch (control.type)
		{
			case Type::masterRequest:
				if (! master)
					return {};
				handoffTo = control.device;
				handoffAsked = true;
				return { { masterResponse (self, name), control.device } };

			case Type::masterResponse:
				if (control.device == requestedFrom)
					requestedAt = now;   // agreed: Mh follows in its next status
				return {};

			case Type::syncCommand:
				// Sync on/off would switch the deck's SYNC; not taken from the link.
				if (control.value == Control::becomeMaster && ! master)
					commanded = true;
				return {};
		}
		return {};
	}

	void TempoMaster::offerToAPlayingFollower (double now)
	{
		const auto stopped = ! (hasTrack && playing);
		if (handoffTo != noHandoff)
		{
			const auto found = peers.find (handoffTo);
			const auto gone = found == peers.end() || ! isFresh (found->second, now);
			const auto offerLapsed = ! handoffAsked
									 && (! stopped || ! found->second.status.isPlaying || ! found->second.status.isSynced);
			if (gone || offerLapsed)
				handoffTo = noHandoff;
		}

		if (handoffTo != noHandoff || ! stopped)
			return;

		for (const auto& [device, peer] : peers)
			if (isFresh (peer, now) && ! peer.status.isMixer && ! peer.status.isMaster
				&& peer.status.isSynced && peer.status.isPlaying)
			{
				handoffTo = device;
				handoffAsked = false;
				return;
			}
	}

	std::vector<TempoMaster::Send> TempoMaster::tick (double now)
	{
		if (master)
		{
			offerToAPlayingFollower (now);
			return {};
		}

		if (! wantsMaster() || now - startedAt < settle)
			return {};

		const auto* current = currentMaster (now);
		if (current == nullptr)
		{
			becomeMaster (now);
			return {};
		}

		const auto holder = current->status.device;
		if (requestedFrom != holder)
		{
			requestedFrom = holder;
			requestedAt = now;
			return { { masterRequest (self, name), holder } };
		}

		if (now - requestedAt >= answerWait)
			becomeMaster (now);   // no answer: the newest claim wins
		return {};
	}
}
