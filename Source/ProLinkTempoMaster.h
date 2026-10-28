// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ProLinkPackets.h"

#include <map>
#include <string>
#include <vector>

namespace prolink
{
	// The tempo master role on the link, passed on as a CDJ-3000 does
	// (sync.html of the DJ Link analysis; CDJ-3000X manual p. 101):
	//   - MASTER pressed with nobody master: the role is taken at once -- once
	//     the link has been heard for `settle`, so a running master is known.
	//   - Without a track, nothing to lead with: not taken, nobody asked.
	//   - With a master on the link: a takeover request goes to it; the role
	//     passes when it names us in its status (Mh). No answer within
	//     `answerWait` (or no Mh within `agreedWait` of its answer): taken
	//     anyway, the newest claim winning (players that predate the handoff).
	//   - A handoff asked for and not taken within `agreedWait` is withdrawn.
	//   - Asked by another player: the response goes back, Mh names it, and
	//     master is let go when it says it is master; Syncn then goes one
	//     above the highest heard.
	//   - Paused, or without a track: a synced, playing player is named in Mh
	//     (the unsolicited handoff).
	//   - Named in another master's Mh: taken, asked or not.
	//   - A newer claim without the handoff: let go. Two claims at once: the
	//     lower player number keeps it.
	// Pure: the caller is the clock and the socket; a Send goes to port 50001
	// of the device it names.
	class TempoMaster
	{
	public:
		struct Send
		{
			std::vector<uint8_t> bytes;
			int to = 0;   // device number
		};
		// What the deck's MASTER key must follow: the role arrived without
		// the DJ asking here, or went to another player.
		enum class Change { none, granted, yielded };

		// agreedWait: how long an agreed handoff may take, either way.
		// concurrent: claims this close together were made before either was
		// heard (a status interval and some); the lower player number keeps it.
		static constexpr double settle = 0.5, answerWait = 1.0, agreedWait = 3.0, silence = 2.0, concurrent = 0.3;

		TempoMaster (int device, std::string name, double startedAt);

		void setDevice (int device) { self = device; }
		// The deck the status reports.
		void setLocal (bool hasTrack, bool playing, bool synced);
		// MASTER on or off, as the DJ set it. On: asked for from tick().
		void want (bool on, double now);

		std::vector<Send> hear (const StatusPacket&, double now);
		std::vector<Send> hear (const Control&, double now);
		std::vector<Send> tick (double now);

		bool isMaster() const { return master; }
		MasterState state() const;
		Change takeChange();

	private:
		struct Peer
		{
			StatusPacket status;
			double last = 0.0, claimSince = -1.0;   // claimSince: master since; -1 not master
		};

		bool wantsMaster() const { return asked || commanded; }
		bool isFresh (const Peer& peer, double now) const { return now - peer.last <= silence; }
		const Peer* currentMaster (double now) const;
		void becomeMaster (double now);
		void giveUp();
		void yield();
		void offerToAPlayingFollower (double now);

		int self;
		std::string name;
		double startedAt;
		bool hasTrack = false, playing = false, synced = false;
		bool asked = false, commanded = false;
		bool master = false;
		double masterSince = 0.0;
		int handoffTo = 0xff;
		bool handoffAsked = false;   // asked for by a takeover request, not offered
		double handoffSince = 0.0;
		int requestedFrom = 0;
		double requestedAt = 0.0;
		bool agreed = false;         // the master answered our request
		uint32_t syncCounter = 0, highestSyncCounter = 0;
		std::map<int, Peer> peers;
		Change change = Change::none;
	};
}
