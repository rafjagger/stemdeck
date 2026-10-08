#include "SurfaceLayout.h"

#include <algorithm>
#include <initializer_list>

namespace surface
{
	//==============================================================================
	bool Rect::contains (const Rect& o) const
	{
		return o.x >= x && o.y >= y && o.right() <= right() && o.bottom() <= bottom();
	}

	bool Rect::intersects (const Rect& o) const
	{
		return ! isEmpty() && ! o.isEmpty() && o.x < right() && x < o.right() && o.y < bottom() && y < o.bottom();
	}

	Rect Rect::reduced (int dx, int dy) const
	{
		return { x + dx, y + dy, std::max (0, w - 2 * dx), std::max (0, h - 2 * dy) };
	}

	Rect Rect::withSizeKeepingCentre (int width, int height) const
	{
		return { x + (w - width) / 2, y + (h - height) / 2, width, height };
	}

	Rect Rect::removeFromTop (int amount)
	{
		amount = std::clamp (amount, 0, std::max (0, h));
		const Rect slice { x, y, w, amount };
		y += amount;
		h -= amount;
		return slice;
	}

	Rect Rect::removeFromBottom (int amount)
	{
		amount = std::clamp (amount, 0, std::max (0, h));
		h -= amount;
		return { x, y + h, w, amount };
	}

	Rect Rect::removeFromLeft (int amount)
	{
		amount = std::clamp (amount, 0, std::max (0, w));
		const Rect slice { x, y, amount, h };
		x += amount;
		w -= amount;
		return slice;
	}

	Rect Rect::removeFromRight (int amount)
	{
		amount = std::clamp (amount, 0, std::max (0, w));
		w -= amount;
		return { x + w, y, amount, h };
	}

	//==============================================================================
	namespace
	{
		// A share of a length, in per mille.
		int share (int length, int perMille) { return length * perMille / 1000; }

		// Two keys side by side, a gap between them.
		void splitPair (Rect row, int gap, Rect& left, Rect& right)
		{
			left = row.removeFromLeft ((row.w - gap) / 2);
			row.removeFromLeft (gap);
			right = row;
		}

		// `count` keys of equal width across a row, a hairline apart.
		template <size_t count>
		void splitRow (Rect row, std::array<Rect, count>& keys)
		{
			const auto width = row.w / (int) count;
			for (auto& key : keys)
				key = row.removeFromLeft (width).reduced (1, 0);
		}
	}

	Sections sections (int width, int height)
	{
		Sections s;
		Rect area { 0, 0, width, height };
		const auto margin = std::max (2, width / 128);
		area = area.reduced (margin);

		s.topBar = area.removeFromTop (share (height, 30));
		area.removeFromTop (margin * 2 / 3);

		// The rolling waveforms, A over B, between the two pitch columns.
		const auto waveHeight = std::clamp (height / 8, 90, 170);
		const auto waveGap = margin / 2;
		s.waves = area.removeFromTop (waveHeight * 2 + waveGap);
		auto waves = s.waves;
		const auto pitchWidth = width / 12;
		s.pitch[0] = waves.removeFromLeft (pitchWidth);
		s.pitch[1] = waves.removeFromRight (pitchWidth);
		waves.removeFromLeft (waveGap);
		waves.removeFromRight (waveGap);
		s.waveA = waves.removeFromTop (waveHeight);
		waves.removeFromTop (waveGap);
		s.waveB = waves;
		area.removeFromTop (margin);

		// Deck A | mixer | deck B. The mixer keeps 400 px at the least, so a
		// deck column keeps ~180 px on the rig's 768 px screen.
		auto middle = area.removeFromTop (std::min (share (area.h, 650), area.h - 150));
		const auto mixerWidth = std::clamp (width / 3, 400, 560);
		const auto deckWidth = (middle.w - mixerWidth) / 2;
		s.deck[0] = middle.removeFromLeft (deckWidth);
		s.deck[1] = middle.removeFromRight (deckWidth);
		s.mixer = middle;
		area.removeFromTop (margin);

		s.library = area;
		return s;
	}

	PitchColumn pitchColumn (Rect local)
	{
		PitchColumn p;
		const auto gap = std::max (2, local.h / 64);
		p.range = local.removeFromBottom (share (local.h, 150));
		local.removeFromBottom (gap);
		p.fader = local;
		p.valueHeight = share (p.fader.h + gap + p.range.h, 80);
		return p;
	}

	DeckColumn deckColumn (Rect local, bool gridOn)
	{
		DeckColumn c;
		auto area = local.reduced (std::max (3, local.w / 30));
		const auto gap = std::max (3, area.h / 80);
		const auto keyHeight = share (area.h, 85);
		const auto height = area.h;

		// From the foot up: the keys, which stay where they are whatever GRID does.
		splitPair (area.removeFromBottom (keyHeight), gap, c.vinyl, c.grid);
		area.removeFromBottom (gap);
		splitPair (area.removeFromBottom (keyHeight), gap, c.sync, c.master);
		area.removeFromBottom (gap);
		splitPair (area.removeFromBottom (keyHeight), gap, c.loopOff, c.repeat);
		area.removeFromBottom (gap);

		// Track search either side of CUE | PLAY, as on a CDJ: a sixth of the
		// row each, so the keys played in time keep most of it.
		auto transport = area.removeFromBottom (share (height, 100));
		const auto stepWidth = transport.w / 6;
		c.previous = transport.removeFromLeft (stepWidth);
		c.next = transport.removeFromRight (stepWidth);
		transport.removeFromLeft (gap);
		transport.removeFromRight (gap);
		splitPair (transport, gap, c.cue, c.play);
		area.removeFromBottom (gap);

		// Grid Adjust while GRID is on, its room taken from the overview.
		if (gridOn)
		{
			const auto gridHeight = share (height, 70);
			splitRow (area.removeFromBottom (gridHeight), c.gridEdit);
			area.removeFromBottom (gap);
			splitRow (area.removeFromBottom (gridHeight), c.gridNudge);
			area.removeFromBottom (gap);
		}

		c.bpmInfo = area.removeFromBottom (share (height, 30));
		c.bpm = area.removeFromBottom (share (height, 60));
		auto times = area.removeFromBottom (share (height, 50));
		c.elapsed = times.removeFromLeft (times.w / 2);
		c.remaining = times;
		area.removeFromBottom (gap);

		c.title = area.removeFromTop (share (height, 50));
		c.stems = area.removeFromTop (share (height, 35));
		area.removeFromTop (gap);
		c.overview = area;
		return c;
	}

	Strip channelStrip (Rect local, int deckIndex, int meterReserve)
	{
		Strip strip;
		auto area = local.reduced (std::max (3, local.w / 40));
		strip.header = area.removeFromTop (share (local.h, 50));
		const auto gap = std::max (2, area.w / 56);
		area.removeFromTop (gap);

		// [knob] over [name] / [1 2 3 4 A  M]: the knob a seventh of the row,
		// the six switches share the rest.
		const auto innerWidth = area.w - 2 * gap;
		const auto knobSize = innerWidth / 7;
		const auto switchWidth = (innerWidth - knobSize - 2 * gap) / 6;
		const auto switchHeight = switchWidth * 11 / 10;
		const auto labelHeight = share (local.h, 28);

		for (size_t s = 0; s < strip.stems.size(); ++s)
		{
			auto& row = strip.stems[s];
			if (s > 0)
				area.removeFromTop (gap);
			row.frame = area.removeFromTop (labelHeight + switchHeight + 2 * gap);
			auto inner = row.frame.reduced (gap);

			row.knob = inner.removeFromLeft (knobSize).withSizeKeepingCentre (knobSize, knobSize);
			inner.removeFromLeft (gap);
			row.label = inner.removeFromTop (labelHeight);

			for (auto& bus : row.buses)
				bus = inner.removeFromLeft (switchWidth).reduced (1, 0);
			inner.removeFromLeft (gap);
			row.mute = inner.reduced (1, 0);
		}

		// Below: the volume fader, the room the rows left; the meter zone on
		// the side towards the mixer's middle, the fader directly beside it.
		area.removeFromTop (2 * gap);
		strip.meterZone = deckIndex == 0 ? area.removeFromRight (meterReserve) : area.removeFromLeft (meterReserve);
		area.removeFromBottom (meterCaptionHeight (strip.meterZone.h));
		strip.fader = area;
		return strip;
	}

	Mixer mixer (Rect local)
	{
		Mixer m;
		auto area = local.reduced (std::max (4, local.w / 50));
		const auto gap = std::max (4, local.w / 50);
		const auto metersWidth = share (area.w, 280);
		m.meterReserve = (metersWidth - gap) / 2;

		const auto stripWidth = (area.w - gap) / 2;
		m.strip[0] = area.removeFromLeft (stripWidth);
		m.strip[1] = area.removeFromRight (stripWidth);

		const auto zoneA = channelStrip ({ 0, 0, m.strip[0].w, m.strip[0].h }, 0, m.meterReserve).meterZone
							   .translated (m.strip[0].x, m.strip[0].y);
		const auto zoneB = channelStrip ({ 0, 0, m.strip[1].w, m.strip[1].h }, 1, m.meterReserve).meterZone
							   .translated (m.strip[1].x, m.strip[1].y);
		m.meters = { zoneA.x, zoneA.y, zoneB.right() - zoneA.x, zoneA.h };
		return m;
	}

	int meterCaptionHeight (int metersHeight)
	{
		return share (metersHeight, 120);
	}
}
