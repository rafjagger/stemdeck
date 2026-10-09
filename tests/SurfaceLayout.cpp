#include <gtest/gtest.h>

#include "SurfaceLayout.h"

#include <string>
#include <utility>
#include <vector>

namespace
{
	using surface::Rect;

	// The rig's screen: a3nuc2's touchscreen, portrait.
	constexpr int rigWidth = 768, rigHeight = 1024;

	// Responsive: the same relations at other windows.
	const std::vector<std::pair<int, int>> windows { { rigWidth, rigHeight }, { 600, 800 }, { 1080, 1920 } };

	std::string sizeName (int w, int h) { return std::to_string (w) + "x" + std::to_string (h); }

	struct Placed
	{
		std::string name;
		Rect bounds;
		bool interactive;
	};

	struct Section
	{
		std::string name;
		Rect bounds;
		std::vector<Placed> controls;
	};

	std::string deckName (int deck) { return deck == 0 ? "A" : "B"; }

	// Every control the surface shows, in window coordinates, with the section
	// it belongs to.
	std::vector<Section> placeAll (int width, int height, std::array<bool, 2> gridOn)
	{
		const auto s = surface::sections (width, height);
		std::vector<Section> all;

		all.push_back ({ "waves", s.waves, { { "waveA", s.waveA, true }, { "waveB", s.waveB, true } } });

		for (int d = 0; d < 2; ++d)
		{
			const auto origin = s.deck[(size_t) d];
			const auto c = surface::deckColumn (Rect { 0, 0, origin.w, origin.h }, gridOn[(size_t) d], d, s.metrics);
			const auto at = [&origin] (const Rect& r) { return r.translated (origin.x, origin.y); };
			Section deck { "deck" + deckName (d), origin, {} };
			const auto add = [&] (const std::string& name, const Rect& r, bool interactive)
			{
				deck.controls.push_back ({ deck.name + "." + name, at (r), interactive });
			};
			add ("elapsed", c.elapsed, false);
			add ("remaining", c.remaining, false);
			add ("transportTile", c.transportTile, false);
			add ("previous", c.previous, true);
			add ("cue", c.cue, true);
			add ("play", c.play, true);
			add ("next", c.next, true);
			add ("loopOff", c.loopOff, true);
			add ("repeat", c.repeat, true);
			add ("sync", c.sync, true);
			add ("master", c.master, true);
			add ("vinyl", c.vinyl, true);
			add ("grid", c.grid, true);

			if (gridOn[(size_t) d])
			{
				for (size_t i = 0; i < c.gridNudge.size(); ++i)
					add ("nudge" + std::to_string (i), c.gridNudge[i], true);
				for (size_t i = 0; i < c.gridEdit.size(); ++i)
					add ("gridEdit" + std::to_string (i), c.gridEdit[i], true);
			}
			all.push_back (deck);
		}

		Section mixer { "mixer", s.mixer, {} };
		const auto strips = surface::mixerStrips (Rect { 0, 0, s.mixer.w, s.mixer.h });
		for (int d = 0; d < 2; ++d)
		{
			const auto stripOrigin = strips[(size_t) d].translated (s.mixer.x, s.mixer.y);
			const auto strip = surface::channelStrip (Rect { 0, 0, stripOrigin.w, stripOrigin.h }, d);
			const auto at = [&] (const Rect& r) { return r.translated (stripOrigin.x, stripOrigin.y); };
			const auto prefix = "strip" + deckName (d) + ".";

			for (size_t stem = 0; stem < strip.stems.size(); ++stem)
			{
				const auto& row = strip.stems[stem];
				const auto stemName = prefix + "stem" + std::to_string (stem) + ".";
				mixer.controls.push_back ({ stemName + "knob", at (row.knob), true });
				mixer.controls.push_back ({ stemName + "mute", at (row.mute), true });
				for (size_t bus = 0; bus < row.buses.size(); ++bus)
					mixer.controls.push_back ({ stemName + "bus" + std::to_string (bus), at (row.buses[bus]), true });
			}
		}
		all.push_back (mixer);

		const auto b = surface::band (s.band, s.metrics);
		Section band { "band", s.band, { { "metersTile", b.metersTile, false }, { "meters", b.meters, false } } };
		for (size_t d = 0; d < 2; ++d)
		{
			const auto name = deckName ((int) d);
			const auto& deck = b.deck[d];
			band.controls.push_back ({ "tile" + name, deck.tile, false });
			band.controls.push_back ({ "pitch" + name, deck.pitch, true });
			band.controls.push_back ({ "pitchValue" + name, deck.pitchValue, false });
			band.controls.push_back ({ "range" + name, deck.range, true });
			band.controls.push_back ({ "bpm" + name, deck.bpm, false });
			band.controls.push_back ({ "bpmInfo" + name, deck.bpmInfo, false });
			band.controls.push_back ({ "title" + name, deck.title, false });
			band.controls.push_back ({ "stems" + name, deck.stems, false });
			band.controls.push_back ({ "overview" + name, deck.overview, true });
			band.controls.push_back ({ "fader" + name, deck.fader, true });
		}
		all.push_back (band);

		all.push_back ({ "library", s.library, {} });
		return all;
	}

	void expectCleanSurface (int width, int height, std::array<bool, 2> gridOn)
	{
		const auto all = placeAll (width, height, gridOn);
		const Rect screen { 0, 0, width, height };
		const auto at = sizeName (width, height);
		std::vector<Placed> interactive;

		for (const auto& section : all)
		{
			EXPECT_TRUE (screen.contains (section.bounds)) << section.name << " leaves the screen at " << at;

			for (const auto& control : section.controls)
			{
				EXPECT_FALSE (control.bounds.isEmpty()) << control.name << " has no size at " << at;
				EXPECT_TRUE (section.bounds.contains (control.bounds)) << control.name << " leaves " << section.name << " at " << at;
				if (control.interactive)
					interactive.push_back (control);
			}
		}

		for (size_t i = 0; i < interactive.size(); ++i)
			for (size_t j = i + 1; j < interactive.size(); ++j)
				EXPECT_FALSE (interactive[i].bounds.intersects (interactive[j].bounds))
					<< interactive[i].name << " overlaps " << interactive[j].name << " at " << at;
	}
}

TEST (SurfaceLayout, TheSectionsDoNotOverlap)
{
	for (const auto& [w, h] : windows)
	{
		const auto s = surface::sections (w, h);
		const std::vector<std::pair<const char*, Rect>> sections {
			{ "topBar", s.topBar }, { "waves", s.waves }, { "deckA", s.deck[0] }, { "deckB", s.deck[1] },
			{ "mixer", s.mixer }, { "band", s.band }, { "library", s.library } };

		for (size_t i = 0; i < sections.size(); ++i)
			for (size_t j = i + 1; j < sections.size(); ++j)
				EXPECT_FALSE (sections[i].second.intersects (sections[j].second))
					<< sections[i].first << " / " << sections[j].first << " at " << sizeName (w, h);
	}
}

TEST (SurfaceLayout, EveryControlInItsSectionNoTwoOverlapAtEverySize)
{
	for (const auto& [w, h] : windows)
		for (const auto grid : { std::array<bool, 2> { false, false }, { true, true }, { true, false }, { false, true } })
			expectCleanSurface (w, h, grid);
}

// The gap and the line are shares of the window, nothing fixed.
TEST (SurfaceLayout, GapAndLineGrowWithTheWindow)
{
	const auto small = surface::metrics (600), rig = surface::metrics (rigWidth), large = surface::metrics (1080);
	EXPECT_LT (small.gap, rig.gap);
	EXPECT_LT (rig.gap, large.gap);
	EXPECT_LE (small.line, rig.line);
	EXPECT_LT (rig.line, large.line);
	EXPECT_GT (small.line, 0);
}

TEST (SurfaceLayout, WaveformsFullWidthBandUnderDecksAndMixer)
{
	for (const auto& [w, h] : windows)
	{
		const auto s = surface::sections (w, h);
		EXPECT_EQ (s.waveA.x, s.waves.x);
		EXPECT_EQ (s.waveA.w, s.waves.w);
		EXPECT_EQ (s.band.x, s.deck[0].x);
		EXPECT_EQ (s.band.right(), s.deck[1].right());
		EXPECT_GT (s.band.y, s.mixer.bottom());
		EXPECT_EQ (s.deck[0].bottom(), s.mixer.bottom());
	}
}

// Each deck's tile: the pitch column at the outer edge (value above the
// fader, range below), the title above the overview, BPM under it, the volume
// fader towards the meters; the meters' tile between the decks' tiles.
TEST (SurfaceLayout, OneTilePerDeckAndOneForTheMeters)
{
	for (const auto& [w, h] : windows)
	{
		const auto at = sizeName (w, h);
		const auto s = surface::sections (w, h);
		const auto b = surface::band (s.band, s.metrics);

		for (size_t d = 0; d < 2; ++d)
		{
			const auto& deck = b.deck[d];
			for (const auto& part : { deck.pitchValue, deck.pitch, deck.range, deck.title, deck.stems,
									  deck.overview, deck.bpm, deck.bpmInfo, deck.fader })
				EXPECT_TRUE (deck.tile.contains (part)) << at;
			EXPECT_FALSE (deck.tile.intersects (b.metersTile)) << at;

			EXPECT_LT (deck.pitchValue.bottom(), deck.pitch.y + 1) << "the value above the fader, " << at;
			EXPECT_LT (deck.pitch.bottom(), deck.range.y + 1) << "the range key below it, " << at;
			EXPECT_LT (deck.title.bottom(), deck.stems.y + 1) << at;
			EXPECT_LT (deck.stems.bottom(), deck.overview.y + 1) << "the title above the overview, " << at;
			EXPECT_LT (deck.overview.bottom(), deck.bpm.y + 1) << "BPM under it, " << at;
			EXPECT_LT (deck.bpm.bottom(), deck.bpmInfo.y + 1) << at;
			EXPECT_EQ (deck.fader.bottom(), b.meters.bottom() - surface::meterCaptionHeight (b.meters.h))
				<< "the fader ends where the meter bars do, " << at;
			EXPECT_EQ (deck.fader.y, b.meters.y) << at;
			EXPECT_EQ (deck.tile.y, b.metersTile.y) << at;
			EXPECT_EQ (deck.tile.h, b.metersTile.h) << at;
		}

		// Pitch at the outer edges, the faders towards the meters.
		EXPECT_LT (b.deck[0].pitch.right(), b.deck[0].overview.x);
		EXPECT_LT (b.deck[0].overview.right(), b.deck[0].fader.x);
		EXPECT_LT (b.deck[1].fader.right(), b.deck[1].overview.x);
		EXPECT_LT (b.deck[1].overview.right(), b.deck[1].pitch.x);
		EXPECT_LT (b.deck[0].tile.right(), b.metersTile.x);
		EXPECT_GT (b.deck[1].tile.x, b.metersTile.right());
		EXPECT_TRUE (b.metersTile.contains (b.meters));
	}
}

// The deck's colour line keeps its path -- stripe, drop, top, side -- and every
// tile stands one gap from it: the deck's tile, the meters' tile, the
// transport's tile in the column. The same gap everywhere, at every size.
TEST (SurfaceLayout, TilesStandOneGapFromTheDeckLines)
{
	for (const auto& [w, h] : windows)
	{
		const auto at = sizeName (w, h);
		const auto s = surface::sections (w, h);
		const auto m = s.metrics;
		const auto b = surface::band (s.band, m);

		for (int d = 0; d < 2; ++d)
		{
			const auto line = surface::deckLine (s, b, d);
			const auto& deck = s.deck[(size_t) d];
			const auto& tile = b.deck[(size_t) d].tile;

			EXPECT_EQ (line.stripe, surface::deckStripe (deck, d, m)) << "the column paints the same stripe";
			EXPECT_TRUE (deck.contains (line.stripe));
			EXPECT_EQ (line.drop.y, line.stripe.bottom()) << "continuous, " << at;
			EXPECT_EQ (line.drop.bottom(), line.top.y);
			EXPECT_EQ (line.side.y, line.top.y);
			EXPECT_EQ (line.side.bottom(), s.band.bottom()) << "down to the band's foot";
			EXPECT_FALSE (line.top.intersects (s.mixer));
			EXPECT_FALSE (line.top.intersects (deck));

			// One gap: above the tiles, between the line and either tile.
			EXPECT_EQ (tile.y - line.top.bottom(), m.gap) << at;
			EXPECT_EQ (b.metersTile.y - line.top.bottom(), m.gap) << at;
			if (d == 0)
			{
				EXPECT_EQ (line.side.x - tile.right(), m.gap) << at;
				EXPECT_EQ (b.metersTile.x - line.side.right(), m.gap) << at;
				EXPECT_EQ (line.top.x, line.stripe.x);
				EXPECT_EQ (line.top.right(), line.side.right());
			}
			else
			{
				EXPECT_EQ (tile.x - line.side.right(), m.gap) << at;
				EXPECT_EQ (line.side.x - b.metersTile.right(), m.gap) << at;
				EXPECT_EQ (line.top.right(), line.stripe.right());
				EXPECT_EQ (line.top.x, line.side.x);
			}

			// The transport's tile, one gap from the stripe.
			const auto column = surface::deckColumn (Rect { 0, 0, deck.w, deck.h }, false, d, m);
			const auto transport = column.transportTile.translated (deck.x, deck.y);
			if (d == 0)
				EXPECT_EQ (line.stripe.x - transport.right(), m.gap) << at;
			else
				EXPECT_EQ (transport.x - line.stripe.right(), m.gap) << at;
			for (const auto& key : { column.previous, column.next, column.cue, column.play })
				EXPECT_TRUE (column.transportTile.reduced (m.gap).contains (key)) << "a gap inside the tile, " << at;
		}
	}
}

// No key moves when GRID turns on: the finger that turned it on turns it off.
TEST (SurfaceLayout, DeckKeysStayPutWithGrid)
{
	for (const auto& [w, h] : windows)
		for (int d = 0; d < 2; ++d)
		{
			const auto s = surface::sections (w, h);
			const Rect column { 0, 0, s.deck[(size_t) d].w, s.deck[(size_t) d].h };
			const auto off = surface::deckColumn (column, false, d, s.metrics);
			const auto on = surface::deckColumn (column, true, d, s.metrics);

			for (const auto& [a, b] : { std::pair { off.previous, on.previous }, { off.cue, on.cue }, { off.play, on.play },
										{ off.next, on.next }, { off.loopOff, on.loopOff }, { off.repeat, on.repeat },
										{ off.sync, on.sync }, { off.master, on.master }, { off.vinyl, on.vinyl }, { off.grid, on.grid } })
				EXPECT_EQ (a, b) << sizeName (w, h);

			EXPECT_LT (on.remaining.bottom(), on.gridNudge[0].y + 1) << "Grid Adjust under the times";
			EXPECT_LT (on.gridEdit[0].bottom(), on.loopOff.y + 1) << "the keys under Grid Adjust";

			// With GRID off the times take the Grid Adjust rows' room: nothing empty.
			EXPECT_LE (off.elapsed.y, on.elapsed.y);
			EXPECT_GE (off.remaining.bottom(), on.gridEdit[0].bottom());
			EXPECT_GT (off.elapsed.h, on.elapsed.h) << "bigger when they have the room";
		}
}

// PLAY, CUE, NEXT and PREV: the biggest keys, at the bottom, within reach --
// a 2 x 2 block, CUE | PLAY at the foot, |< | >| over them.
TEST (SurfaceLayout, TheTransportIsTheBiggestAndLowest)
{
	for (const auto& [w, h] : windows)
	{
		const auto s = surface::sections (w, h);
		for (const auto gridOn : { false, true })
		{
			const auto c = surface::deckColumn (Rect { 0, 0, s.deck[0].w, s.deck[0].h }, gridOn, 0, s.metrics);
			std::vector<Rect> others { c.loopOff, c.repeat, c.sync, c.master, c.vinyl, c.grid };
			others.insert (others.end(), c.gridNudge.begin(), c.gridNudge.end());
			others.insert (others.end(), c.gridEdit.begin(), c.gridEdit.end());

			for (const auto& key : { c.previous, c.next, c.cue, c.play })
				for (const auto& other : others)
				{
					EXPECT_GT (key.h, other.h) << sizeName (w, h);
					EXPECT_GT (key.w * key.h, other.w * other.h) << "a larger target, " << sizeName (w, h);
					EXPECT_GT (key.y, other.y) << "below every other key";
				}

			EXPECT_EQ (c.cue.y, c.play.y);
			EXPECT_EQ (c.previous.y, c.next.y);
			EXPECT_LT (c.previous.bottom(), c.cue.y + 1) << "track search over CUE | PLAY";
			EXPECT_EQ (c.cue.x, c.previous.x);
			EXPECT_EQ (c.play.right(), c.next.right());
			EXPECT_GE (c.transportTile.bottom(), s.deck[0].h - s.deck[0].h / 20) << "at the column's foot";
		}
	}
}

TEST (SurfaceLayout, DeckKeysAreAtLeastFingerHighOnTheRig)
{
	const auto s = surface::sections (rigWidth, rigHeight);
	const auto c = surface::deckColumn (Rect { 0, 0, s.deck[0].w, s.deck[0].h }, true, 0, s.metrics);

	for (const auto& key : { c.cue, c.play, c.previous, c.next })
		EXPECT_GE (key.h, 40);
	for (const auto& key : { c.loopOff, c.sync, c.vinyl, c.grid })
		EXPECT_GE (key.h, 28);
	for (const auto& key : c.gridNudge)
		EXPECT_GE (key.h, 22);
	EXPECT_GE (surface::band (s.band, s.metrics).deck[0].range.h, 26);
}

// The mixer's stem rows: the knob half its old size, the bus switches and
// mute in one row, the four rows tight under each other -- a matrix of 4 x 6
// keys per deck, the two decks' matrices apart.
TEST (SurfaceLayout, StemRowsFormAMatrix)
{
	for (const auto& [w, h] : windows)
	{
		const auto s = surface::sections (w, h);
		const auto strips = surface::mixerStrips (Rect { 0, 0, s.mixer.w, s.mixer.h });
		const auto strip = surface::channelStrip (Rect { 0, 0, strips[0].w, strips[0].h }, 0);

		for (size_t r = 0; r < strip.stems.size(); ++r)
		{
			const auto& row = strip.stems[r];
			for (const auto& bus : row.buses)
			{
				EXPECT_EQ (bus.y, row.buses[0].y);
				EXPECT_EQ (bus.h, row.mute.h);
			}
			EXPECT_GT (row.mute.x, row.buses.back().right());
			EXPECT_TRUE (row.frame.contains (row.mute));

			if (r + 1 < strip.stems.size())
			{
				const auto& next = strip.stems[r + 1];
				EXPECT_EQ (row.frame.bottom(), next.frame.y) << "no gap between the rows";
				for (size_t k = 0; k < row.buses.size(); ++k)
					EXPECT_EQ (row.buses[k].x, next.buses[k].x) << "columns line up";
			}
		}
		EXPECT_GT (strips[1].x - strips[0].right(), 0) << "deck A's and deck B's matrices apart";
	}

	const auto s = surface::sections (rigWidth, rigHeight);
	const auto strips = surface::mixerStrips (Rect { 0, 0, s.mixer.w, s.mixer.h });
	const auto strip = surface::channelStrip (Rect { 0, 0, strips[0].w, strips[0].h }, 0);
	EXPECT_LE (strip.stems[0].knob.w, 28) << "half the 48 px knob";
	EXPECT_GE (strip.stems[0].buses[0].h, 30) << "the keys grew into the freed height";
}

// The keys in the same order as the panel's pads: deck A [knob] AUX 1 2 3 4
// M, deck B [knob] 1 2 3 4 AUX M -- AUX on the deck's outer side, mute last.
TEST (SurfaceLayout, StemRowsPutAuxOnTheOuterSide)
{
	for (const auto& [w, h] : windows)
	{
		const auto s = surface::sections (w, h);
		const auto strips = surface::mixerStrips (Rect { 0, 0, s.mixer.w, s.mixer.h });
		for (int d = 0; d < 2; ++d)
		{
			const auto strip = surface::channelStrip (Rect { 0, 0, strips[(size_t) d].w, strips[(size_t) d].h }, d);
			for (const auto& row : strip.stems)
			{
				const std::array<int, 5> leftToRight = d == 0 ? std::array<int, 5> { buses::aux, 0, 1, 2, 3 }
															  : std::array<int, 5> { 0, 1, 2, 3, buses::aux };
				for (size_t k = 0; k + 1 < leftToRight.size(); ++k)
					EXPECT_LE (row.buses[(size_t) leftToRight[k]].right(), row.buses[(size_t) leftToRight[k + 1]].x)
						<< "deck " << d << ": column " << k << " left of column " << k + 1;
				EXPECT_LE (row.knob.right(), row.buses[(size_t) leftToRight.front()].x);
				EXPECT_GE (row.mute.x, row.buses[(size_t) leftToRight.back()].right());
			}
		}
	}
}

TEST (SurfaceLayout, RectSlicing)
{
	Rect r { 10, 20, 100, 50 };
	EXPECT_EQ (r.removeFromTop (10), (Rect { 10, 20, 100, 10 }));
	EXPECT_EQ (r, (Rect { 10, 30, 100, 40 }));
	EXPECT_EQ (r.removeFromRight (30), (Rect { 80, 30, 30, 40 }));
	EXPECT_EQ (r.removeFromBottom (100), (Rect { 10, 30, 70, 40 })) << "clamped to what is there";
	EXPECT_TRUE (r.isEmpty());
	EXPECT_FALSE ((Rect { 0, 0, 10, 10 }).intersects (Rect { 10, 0, 10, 10 })) << "touching is not overlapping";
	EXPECT_TRUE ((Rect { 0, 0, 10, 10 }).contains (Rect { 2, 2, 8, 8 }));
}
