// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ProLinkAnnouncer.h"

#include <arpa/inet.h>

namespace prolink
{
	namespace
	{
		constexpr int perStage = 3, startupPackets = 4 * perStage;   // hello, then three claim stages
		constexpr int finalStageStart = 3 * perStage;

		std::string dotted (uint32_t ipNetworkOrder)
		{
			in_addr address {};
			address.s_addr = ipNetworkOrder;
			char text[INET_ADDRSTRLEN] {};
			inet_ntop (AF_INET, &address, text, sizeof (text));
			return text;
		}
	}

	Announcer::Announcer (Identity identity) : self (std::move (identity)) {}

	std::vector<uint8_t> Announcer::claimPacket (int index) const
	{
		const auto counter = index % perStage + 1;
		switch (index / perStage)
		{
			case 0: return hello (self);
			case 1: return claimStage1 (self, counter);
			case 2: return claimStage2 (self, counter);
			default: return claimStage3 (self, counter);
		}
	}

	std::vector<Announcer::Send> Announcer::due (double now)
	{
		if (phase == Phase::refused)
			return {};
		if (! started)
		{
			started = true;
			nextAt = now;
		}
		if (now < nextAt)
			return {};

		std::vector<Send> out;
		if (sent < startupPackets)
		{
			out.push_back ({ claimPacket (sent++), 0 });
			nextAt += claimStep;
		}
		else
		{
			if (phase == Phase::claiming)
			{
				phase = Phase::online;
				firstOnNetwork = ! othersHeard;
			}
			out.push_back ({ keepAlive (self, peers (now), firstOnNetwork), 0 });
			nextAt += keepAliveEvery;
		}
		// Late (a busy thread): from now on, not a burst to catch up.
		if (nextAt < now)
			nextAt = now;
		return out;
	}

	bool Announcer::isEcho (const Announcement& heard, uint32_t from) const
	{
		return from == self.ip && (heard.device == self.device || heard.device == 0);
	}

	void Announcer::refuse (uint32_t holder)
	{
		phase = Phase::refused;
		refused = "player " + std::to_string (self.device) + " is taken by " + dotted (holder);
	}

	std::vector<Announcer::Send> Announcer::hear (const uint8_t* data, size_t size, uint32_t from, double now)
	{
		const auto heard = parseAnnouncement (data, size);
		if (! heard || isEcho (*heard, from) || phase == Phase::refused)
			return {};

		using Type = Announcement::Type;
		othersHeard = true;
		if (heard->type == Type::keepAlive && heard->device != self.device)
			lastHeard[heard->device] = now;

		const auto claimsOurs = heard->device == self.device
							 && (heard->type == Type::claim || heard->type == Type::finalClaim || heard->type == Type::keepAlive);

		if (phase == Phase::claiming)
		{
			if ((heard->type == Type::conflict && heard->device == self.device) || (claimsOurs && heard->type == Type::keepAlive))
				refuse (heard->ip != 0 ? heard->ip : from);
			else if (heard->type == Type::assignmentFinished && sent > finalStageStart)
				sent = startupPackets;   // the rest of the final stage is cut short
			return {};
		}

		if (claimsOurs)
			return { { conflict (self), from } };
		if (heard->type == Type::finalClaim)
			return { { assignmentFinished (self), from } };
		return {};
	}

	int Announcer::peers (double now) const
	{
		int count = 1;
		for (const auto& [device, at] : lastHeard)
			if (now - at < peerTimeout)
				++count;
		return count;
	}
}
