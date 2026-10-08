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
		constexpr int timesUnits = 550, gridUnits = 830, transportUnits = 1100, keyUnits = 1000, gapUnits = 120;
		constexpr int deckUnits = timesUnits + 2 * gridUnits + transportUnits + 3 * keyUnits
								+ 8 * gapUnits;   // 6 between rows, 2 margins
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

	namespace
	{
		// One deck's half of the band, from its outer edge (`outer`) to its
		// fader frame; `mirrored` for B, whose outer edge is on the right.
		BandDeck bandDeck (Rect side, int gap, int pitchWidth, int line, int info, bool mirrored)
		{
			BandDeck d;
			const auto outer = [mirrored] (Rect& r, int w) { return mirrored ? r.removeFromRight (w) : r.removeFromLeft (w); };

			auto foot = side.removeFromBottom (line);
			side.removeFromBottom (gap);
			d.pitch = outer (side, pitchWidth);
			outer (side, gap);
			auto infoArea = side.removeFromBottom (info);
			d.title = infoArea.removeFromTop (infoArea.h * 3 / 5);
			d.stems = infoArea;
			side.removeFromBottom (gap);
			d.overview = side;

			d.pitchValue = outer (foot, pitchWidth);
			outer (foot, gap);
			d.range = outer (foot, pitchWidth);
			outer (foot, gap);
			d.bpm = foot.removeFromTop (foot.h * 2 / 3);   // big, the original under it
			d.bpmInfo = foot;
			return d;
		}
	}

	Band band (Rect area)
	{
		Band b;
		const auto gap = std::max (3, area.w / 150);
		const auto pitchWidth = share (area.w, 85);
		const auto metersWidth = share (area.w, 150);
		const auto faderWidth = share (area.w, 75);
		const auto frameWidth = faderWidth + 2 * gap;

		// The middle: the meters, a gap, each deck's fader in its colour frame.
		b.meters = { area.x + (area.w - metersWidth) / 2, area.y, metersWidth, area.h };
		b.faderFrame[0] = { b.meters.x - gap - frameWidth, area.y, frameWidth, area.h };
		b.faderFrame[1] = { b.meters.right() + gap, area.y, frameWidth, area.h };
		const auto faderBottom = area.bottom() - meterCaptionHeight (area.h);
		for (size_t d = 0; d < 2; ++d)
			b.fader[d] = { b.faderFrame[d].x + gap, area.y + gap, faderWidth, faderBottom - area.y - gap };

		const auto line = share (area.h, 170);
		const auto info = share (area.h, 150);
		const Rect sideA { area.x, area.y, b.faderFrame[0].x - gap - area.x, area.h };
		const Rect sideB { b.faderFrame[1].right() + gap, area.y, area.right() - b.faderFrame[1].right() - gap, area.h };
		b.deck[0] = bandDeck (sideA, gap, pitchWidth, line, info, false);
		b.deck[1] = bandDeck (sideB, gap, pitchWidth, line, info, true);
		return b;
	}

	namespace
	{
		constexpr int lineWidth = 3;   // the deck column's stripe, since before the clean surface
		constexpr int panelInset = 2;  // the column's panel stands this far inside its bounds
	}

	Rect deckStripe (Rect deck, int deckIndex)
	{
		const auto panel = deck.reduced (panelInset);
		return { deckIndex == 0 ? panel.right() - lineWidth : panel.x, panel.y, lineWidth, panel.h };
	}

	DeckLine deckLine (const Sections& sections, const Band& band, int deckIndex)
	{
		DeckLine line;
		const auto& frame = band.faderFrame[(size_t) deckIndex];
		line.stripe = deckStripe (sections.deck[(size_t) deckIndex], deckIndex);

		// The top runs in the middle of the gap between the column and the band.
		const auto gapTop = sections.deck[(size_t) deckIndex].bottom();
		const auto topY = gapTop + (sections.band.y - gapTop - lineWidth) / 2;
		line.drop = { line.stripe.x, line.stripe.bottom(), lineWidth, topY - line.stripe.bottom() };

		const auto sideX = deckIndex == 0 ? frame.right() - lineWidth : frame.x;
		line.side = { sideX, topY, lineWidth, sections.band.bottom() - topY };
		line.top = deckIndex == 0 ? Rect { line.stripe.x, topY, line.side.right() - line.stripe.x, lineWidth }
								  : Rect { sideX, topY, line.stripe.right() - sideX, lineWidth };
		return line;
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

		// Grid Adjust above the transport; with GRID off its room is the times'.
		auto times = area;
		splitRow (area.removeFromBottom (units (gridUnits)), c.gridEdit);
		area.removeFromBottom (gap);
		splitRow (area.removeFromBottom (units (gridUnits)), c.gridNudge);
		area.removeFromBottom (gap);

		if (gridOn)
		{
			times = area;
			c.elapsed = times.removeFromLeft (times.w / 2);
			c.remaining = times;
		}
		else
		{
			c.elapsed = times.removeFromTop (times.h / 2);
			c.remaining = times;
		}
		return c;
	}

	Strip channelStrip (Rect local)
	{
		Strip strip;
		auto area = local.reduced (std::max (3, local.w / 40));
		strip.header = area.removeFromTop (share (local.h, 90));
		const auto gap = std::max (2, area.w / 56);
		area.removeFromTop (gap);

		// [knob] [1 2 3 4 A  M], four rows tight under each other: a matrix.
		// The knob a seventh of the row, the six keys share the rest and are
		// as tall as the row, a hairline apart.
		const auto knobSize = area.w / 7;
		const auto switchWidth = (area.w - knobSize - 2 * gap) / 6;
		const auto rows = (int) strip.stems.size();
		const auto rowHeight = area.h / rows;

		for (auto& row : strip.stems)
		{
			row.frame = area.removeFromTop (rowHeight);
			auto inner = row.frame;
			row.knob = inner.removeFromLeft (knobSize).withSizeKeepingCentre (knobSize, knobSize);
			inner.removeFromLeft (gap);
			inner = inner.reduced (0, 1);

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
		const auto gap = 2 * pad;   // the two decks' matrices clearly apart
		const auto stripWidth = (area.w - gap) / 2;
		return { area.removeFromLeft (stripWidth), area.removeFromRight (stripWidth) };
	}

	int meterCaptionHeight (int metersHeight)
	{
		return share (metersHeight, 90);
	}
}
