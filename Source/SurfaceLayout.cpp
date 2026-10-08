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

	namespace
	{
		// A deck column in thousandths of a key: the rows and the gaps between them.
		constexpr int titleUnits = 550, stemsUnits = 400, timesUnits = 550, bpmUnits = 720, bpmInfoUnits = 330;
		constexpr int gridUnits = 830, transportUnits = 1100, keyUnits = 1000, gapUnits = 120;
		constexpr int deckUnits = titleUnits + stemsUnits + timesUnits + bpmUnits + bpmInfoUnits + 2 * gridUnits
								+ transportUnits + 3 * keyUnits + 9 * gapUnits;   // 7 between rows, 2 margins
	}

	int deckColumnHeight (int keyHeight)
	{
		return keyHeight * deckUnits / 1000;
	}

	Sections sections (int width, int height)
	{
		Sections s;
		Rect area { 0, 0, width, height };
		const auto margin = std::max (2, width / 128);
		area = area.reduced (margin);

		s.topBar = area.removeFromTop (share (height, 30));
		area.removeFromTop (margin * 2 / 3);

		// The rolling waveforms, A over B, the whole width.
		const auto waveHeight = std::clamp (height / 9, 80, 170);
		const auto waveGap = margin / 2;
		s.waves = area.removeFromTop (waveHeight * 2 + waveGap);
		auto waves = s.waves;
		s.waveA = waves.removeFromTop (waveHeight);
		waves.removeFromTop (waveGap);
		s.waveB = waves;
		area.removeFromTop (margin);

		s.library = area.removeFromBottom (share (area.h, 300));
		area.removeFromBottom (margin);

		// Deck A | mixer | deck B, as tall as a deck's keys need; the band
		// takes the rest. The mixer keeps 400 px at the least, so a deck
		// column keeps ~180 px on the rig's 768 px screen.
		auto upper = area.removeFromTop (deckColumnHeight (share (height, 36)));
		area.removeFromTop (margin);
		s.band = area;

		const auto mixerWidth = std::clamp (width / 3, 400, 560);
		const auto deckWidth = (upper.w - mixerWidth) / 2;
		s.deck[0] = upper.removeFromLeft (deckWidth);
		s.deck[1] = upper.removeFromRight (deckWidth);
		s.mixer = upper;
		return s;
	}

	Band band (Rect area)
	{
		Band b;
		const auto gap = std::max (3, area.w / 150);
		const auto line = share (area.h, 190);
		const auto pitchWidth = share (area.w, 85);
		const auto metersWidth = share (area.w, 150);
		const auto faderWidth = share (area.w, 75);
		const auto upperHeight = area.h - line - gap;
		const auto lineY = area.bottom() - line;

		b.meters = { area.x + (area.w - metersWidth) / 2, area.y, metersWidth, area.h };
		const auto faderHeight = area.h - meterCaptionHeight (area.h);
		b.fader[0] = { b.meters.x - faderWidth, area.y, faderWidth, faderHeight };
		b.fader[1] = { b.meters.right(), area.y, faderWidth, faderHeight };

		b.pitch[0] = { area.x, area.y, pitchWidth, upperHeight };
		b.pitch[1] = { area.right() - pitchWidth, area.y, pitchWidth, upperHeight };
		b.pitchValue[0] = { area.x, lineY, pitchWidth, line };
		b.pitchValue[1] = { area.right() - pitchWidth, lineY, pitchWidth, line };
		b.range[0] = { b.pitchValue[0].right() + gap, lineY, pitchWidth, line };
		b.range[1] = { b.pitchValue[1].x - gap - pitchWidth, lineY, pitchWidth, line };

		const auto overviewA = b.pitch[0].right() + gap;
		b.overview[0] = { overviewA, area.y, b.fader[0].x - gap - overviewA, upperHeight };
		const auto overviewB = b.fader[1].right() + gap;
		b.overview[1] = { overviewB, area.y, b.pitch[1].x - gap - overviewB, upperHeight };
		return b;
	}

	DeckColumn deckColumn (Rect local, bool gridOn)
	{
		DeckColumn c;
		const auto key = local.h * 1000 / deckUnits;
		const auto units = [key] (int u) { return key * u / 1000; };
		const auto gap = units (gapUnits);
		auto area = local.reduced (std::max (3, local.w / 30), gap);

		// From the foot up: the keys, which stay where they are whatever GRID does.
		splitPair (area.removeFromBottom (units (keyUnits)), gap, c.vinyl, c.grid);
		area.removeFromBottom (gap);
		splitPair (area.removeFromBottom (units (keyUnits)), gap, c.sync, c.master);
		area.removeFromBottom (gap);
		splitPair (area.removeFromBottom (units (keyUnits)), gap, c.loopOff, c.repeat);
		area.removeFromBottom (gap);

		// Track search either side of CUE | PLAY, as on a CDJ: a sixth of the
		// row each, so the keys played in time keep most of it.
		auto transport = area.removeFromBottom (units (transportUnits));
		const auto stepWidth = transport.w / 6;
		c.previous = transport.removeFromLeft (stepWidth);
		c.next = transport.removeFromRight (stepWidth);
		transport.removeFromLeft (gap);
		transport.removeFromRight (gap);
		splitPair (transport, gap, c.cue, c.play);
		area.removeFromBottom (gap);

		// Grid Adjust: its room is kept; with GRID off, times and BPM use it.
		auto labels = area;
		splitRow (area.removeFromBottom (units (gridUnits)), c.gridEdit);
		area.removeFromBottom (gap);
		splitRow (area.removeFromBottom (units (gridUnits)), c.gridNudge);
		area.removeFromBottom (gap);
		if (gridOn)
			labels = area;

		c.bpmInfo = labels.removeFromBottom (units (bpmInfoUnits));
		c.bpm = labels.removeFromBottom (units (bpmUnits));
		auto times = labels.removeFromBottom (units (timesUnits));
		c.elapsed = times.removeFromLeft (times.w / 2);
		c.remaining = times;

		c.title = labels.removeFromTop (units (titleUnits));
		c.stems = labels.removeFromTop (units (stemsUnits));
		return c;
	}

	Strip channelStrip (Rect local)
	{
		Strip strip;
		auto area = local.reduced (std::max (3, local.w / 40));
		strip.header = area.removeFromTop (share (local.h, 70));
		const auto gap = std::max (2, area.w / 56);
		area.removeFromTop (gap);

		// [knob] beside [name] over [1 2 3 4 A  M]: the knob a seventh of the
		// row, the six switches share the rest; the rows share the height.
		const auto innerWidth = area.w - 2 * gap;
		const auto knobSize = innerWidth / 7;
		const auto switchWidth = (innerWidth - knobSize - 2 * gap) / 6;
		const auto labelHeight = share (local.h, 40);
		const auto rows = (int) strip.stems.size();
		const auto frameHeight = (area.h - (rows - 1) * gap) / rows;

		for (size_t s = 0; s < strip.stems.size(); ++s)
		{
			auto& row = strip.stems[s];
			if (s > 0)
				area.removeFromTop (gap);
			row.frame = area.removeFromTop (frameHeight);
			auto inner = row.frame.reduced (gap);
			const auto switchHeight = std::min (inner.h - labelHeight, switchWidth * 14 / 10);

			row.knob = inner.removeFromLeft (knobSize).withSizeKeepingCentre (knobSize, knobSize);
			inner.removeFromLeft (gap);
			inner = inner.withSizeKeepingCentre (inner.w, labelHeight + switchHeight);
			row.label = inner.removeFromTop (labelHeight);

			for (auto& bus : row.buses)
				bus = inner.removeFromLeft (switchWidth).reduced (1, 0);
			inner.removeFromLeft (gap);
			row.mute = inner.reduced (1, 0);
		}
		return strip;
	}

	std::array<Rect, 2> mixerStrips (Rect local)
	{
		const auto pad = std::max (4, local.w / 50);
		auto area = local.reduced (pad);
		const auto stripWidth = (area.w - pad) / 2;
		return { area.removeFromLeft (stripWidth), area.removeFromRight (stripWidth) };
	}

	int meterCaptionHeight (int metersHeight)
	{
		return share (metersHeight, 120);
	}
}
