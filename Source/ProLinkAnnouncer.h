// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ProLinkPackets.h"

#include <map>
#include <string>
#include <vector>

namespace prolink
{
	// A player's side of port 50000, as a CDJ-3000 runs it (startup.html of
	// the DJ Link analysis): three hellos, then three claims in each of three
	// stages, 300 ms apart; settled, a keep-alive every two seconds. It
	// defends its number and finishes other players' claims. Pure: the caller
	// is the clock and the socket.
	//
	// StemDeck's number is fixed per deck, so a number already taken is given
	// up for good: no keep-alive, and refusal() says by whom. On a mixer's
	// channel port the mixer assigns it instead, as it does a CDJ's.
	class Announcer
	{
	public:
		struct Send
		{
			std::vector<uint8_t> bytes;
			uint32_t to = 0;   // network byte order; 0 = the broadcast address
		};

		// mixerWait: how long a mixer's intention holds the own claim back.
		static constexpr double claimStep = 0.3, keepAliveEvery = 2.0, peerTimeout = 10.0, mixerWait = 1.0;

		explicit Announcer (Identity);

		// What is due at `now` (seconds, any monotonic clock): at most one packet.
		std::vector<Send> due (double now);

		// A packet heard on port 50000 from `from`; the replies it calls for.
		std::vector<Send> hear (const uint8_t* data, size_t size, uint32_t from, double now);

		bool isOnline() const { return phase == Phase::online; }
		// The player number: the fixed one, or one a mixer channel port assigned.
		int device() const { return self.device; }
		const std::string& refusal() const { return refused; }

	private:
		enum class Phase { claiming, online, refused };

		std::vector<uint8_t> claimPacket (int index) const;
		int peers (double now) const;
		bool isEcho (const Announcement&, uint32_t from) const;
		void refuse (uint32_t holder);
		std::vector<Send> expectAssignment (uint32_t mixer, double now);
		void takeAssigned (int device, double now);

		Identity self;
		Phase phase = Phase::claiming;
		int sent = 0;                  // startup packets out, 0..12
		bool started = false;
		double nextAt = 0.0;
		bool othersHeard = false;
		bool firstOnNetwork = true;    // latched when the claim ends
		std::map<int, double> lastHeard;   // device -> last keep-alive
		uint32_t assigningMixer = 0;   // a mixer channel port about to assign the number
		double assigningSince = 0.0;
		std::string refused;
	};
}
