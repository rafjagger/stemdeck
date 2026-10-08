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
			const auto c = surface::deckColumn (Rect { 0, 0, origin.w, origin.h }, gridOn[(size_t) d]);
			const auto at = [&origin] (const Rect& r) { return r.translated (origin.x, origin.y); };
			Section deck { "deck" + deckName (d), origin, {} };
			const auto add = [&] (const std::string& name, const Rect& r, bool interactive)
			{
				deck.controls.push_back ({ deck.name + "." + name, at (r), interactive });
			};
			add ("elapsed", c.elapsed, false);
			add ("remaining", c.remaining, false);
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
			const auto strip = surface::channelStrip (Rect { 0, 0, stripOrigin.w, stripOrigin.h });
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

		const auto b = surface::band (s.band);
		Section band { "band", s.band, { { "meters", b.meters, false } } };
		for (size_t d = 0; d < 2; ++d)
		{
			const auto name = deckName ((int) d);
			const auto& deck = b.deck[d];
			band.controls.push_back ({ "pitch" + name, deck.pitch, true });
			band.controls.push_back ({ "pitchValue" + name, deck.pitchValue, false });
			band.controls.push_back ({ "range" + name, deck.range, true });
			band.controls.push_back ({ "bpm" + name, deck.bpm, false });
			band.controls.push_back ({ "bpmInfo" + name, deck.bpmInfo, false });
			band.controls.push_back ({ "title" + name, deck.title, false });
			band.controls.push_back ({ "stems" + name, deck.stems, false });
			band.controls.push_back ({ "overview" + name, deck.overview, true });
			band.controls.push_back ({ "fader" + name, b.fader[d], true });
			band.controls.push_back ({ "faderFrame" + name, b.faderFrame[d], false });
		}
		all.push_back (band);

		all.push_back ({ "library", s.library, {} });
		return all;
	}

	void expectCleanSurface (std::array<bool, 2> gridOn)
	{
		const auto all = placeAll (rigWidth, rigHeight, gridOn);
		const Rect screen { 0, 0, rigWidth, rigHeight };
		std::vector<Placed> interactive;

		for (const auto& section : all)
		{
			EXPECT_TRUE (screen.contains (section.bounds)) << section.name << " leaves the screen";

			for (const auto& control : section.controls)
			{
				EXPECT_FALSE (control.bounds.isEmpty()) << control.name << " has no size";
				EXPECT_TRUE (section.bounds.contains (control.bounds)) << control.name << " leaves " << section.name;
				if (control.interactive)
					interactive.push_back (control);
			}
		}

		for (size_t i = 0; i < interactive.size(); ++i)
			for (size_t j = i + 1; j < interactive.size(); ++j)
				EXPECT_FALSE (interactive[i].bounds.intersects (interactive[j].bounds))
					<< interactive[i].name << " overlaps " << interactive[j].name;
	}
}

TEST (SurfaceLayout, TheSectionsDoNotOverlap)
{
	const auto s = surface::sections (rigWidth, rigHeight);
	const std::vector<std::pair<const char*, Rect>> sections {
		{ "topBar", s.topBar }, { "waves", s.waves }, { "deckA", s.deck[0] }, { "deckB", s.deck[1] },
		{ "mixer", s.mixer }, { "band", s.band }, { "library", s.library } };

	for (size_t i = 0; i < sections.size(); ++i)
		for (size_t j = i + 1; j < sections.size(); ++j)
			EXPECT_FALSE (sections[i].second.intersects (sections[j].second)) << sections[i].first << " / " << sections[j].first;
}

TEST (SurfaceLayout, EveryControlInItsSectionNoTwoOverlapGridOff)
{
	expectCleanSurface ({ false, false });
}

TEST (SurfaceLayout, EveryControlInItsSectionNoTwoOverlapGridOn)
{
	expectCleanSurface ({ true, true });
	expectCleanSurface ({ true, false });
	expectCleanSurface ({ false, true });
}

// The rolling waveforms have the whole width again; the band spans the
// decks and the mixer, right under them.
TEST (SurfaceLayout, WaveformsFullWidthBandUnderDecksAndMixer)
{
	const auto s = surface::sections (rigWidth, rigHeight);
	EXPECT_EQ (s.waveA.x, s.waves.x);
	EXPECT_EQ (s.waveA.w, s.waves.w);
	EXPECT_EQ (s.band.x, s.deck[0].x);
	EXPECT_EQ (s.band.right(), s.deck[1].right());
	EXPECT_GT (s.band.y, s.mixer.bottom() - 1);
	EXPECT_EQ (s.deck[0].bottom(), s.mixer.bottom());
}

// Pitch at the outer edges, beside the overview and the deck's title line;
// under the overview the title, under both the value, range key and BPM; the
// overview reaches to the volume fader's colour frame, the meters between.
TEST (SurfaceLayout, TheBandRunsPitchOverviewFaderMeters)
{
	const auto s = surface::sections (rigWidth, rigHeight);
	const auto b = surface::band (s.band);

	EXPECT_EQ (b.deck[0].pitch.x, s.band.x);
	EXPECT_EQ (b.deck[1].pitch.right(), s.band.right());
	for (size_t d = 0; d < 2; ++d)
	{
		const auto& deck = b.deck[d];
		EXPECT_EQ (deck.pitch.y, deck.overview.y);
		EXPECT_EQ (deck.pitch.bottom(), deck.stems.bottom()) << "the fader beside overview and title";
		EXPECT_LT (deck.overview.bottom(), deck.title.y + 1) << "the title under the overview";
		EXPECT_LT (deck.title.bottom(), deck.stems.y + 1);
		EXPECT_EQ (deck.title.x, deck.overview.x);
		EXPECT_EQ (deck.title.w, deck.overview.w);
		for (const auto& foot : { deck.pitchValue, deck.range, deck.bpm, deck.bpmInfo })
			EXPECT_LT (deck.stems.bottom(), foot.y + 1) << "the bottom line under it all";
		EXPECT_EQ (deck.bpm.y, deck.range.y);
		EXPECT_EQ (deck.bpmInfo.bottom(), deck.range.bottom()) << "the original under the BPM";
		EXPECT_GE (deck.bpm.w, 90) << "room for 129.00, big";
		EXPECT_GE (deck.overview.h, 120);
		EXPECT_GE (deck.range.h, 30);
		EXPECT_TRUE (b.faderFrame[d].contains (b.fader[d]));
		EXPECT_FALSE (b.faderFrame[d].intersects (b.meters)) << "the frame separates fader and meters";
		EXPECT_FALSE (b.faderFrame[d].intersects (deck.overview));
		EXPECT_FALSE (b.faderFrame[d].intersects (deck.bpmInfo));
		EXPECT_EQ (b.faderFrame[d].h, s.band.h);
	}
	EXPECT_EQ (b.deck[0].pitchValue.x, b.deck[0].pitch.x) << "the value under the pitch fader";
	EXPECT_EQ (b.deck[1].pitchValue.right(), b.deck[1].pitch.right());
	EXPECT_LT (b.deck[0].pitch.right(), b.deck[0].overview.x);
	EXPECT_LT (b.deck[0].overview.right(), b.faderFrame[0].x);
	EXPECT_LT (b.faderFrame[0].right(), b.meters.x);
	EXPECT_GT (b.faderFrame[1].x, b.meters.right());
	EXPECT_LT (b.faderFrame[1].right(), b.deck[1].overview.x);
	EXPECT_LT (b.deck[1].overview.right(), b.deck[1].pitch.x);

	EXPECT_EQ (b.fader[0].bottom(), b.meters.bottom() - surface::meterCaptionHeight (b.meters.h))
		<< "the fader ends where the meter bars do";
	EXPECT_GE (b.meters.w, 100);
	EXPECT_LE (b.meters.w, 125);
	EXPECT_GE (b.fader[0].h, 160) << "the faders keep a usable travel";
	EXPECT_GE (b.deck[0].overview.w, 170);
}

// No key moves when GRID turns on: the finger that turned it on turns it off.
TEST (SurfaceLayout, DeckKeysStayPutWithGrid)
{
	const auto s = surface::sections (rigWidth, rigHeight);
	const Rect column { 0, 0, s.deck[0].w, s.deck[0].h };
	const auto off = surface::deckColumn (column, false);
	const auto on = surface::deckColumn (column, true);

	for (const auto& [a, b] : { std::pair { off.previous, on.previous }, { off.cue, on.cue }, { off.play, on.play },
								{ off.next, on.next }, { off.loopOff, on.loopOff }, { off.repeat, on.repeat },
								{ off.sync, on.sync }, { off.master, on.master }, { off.vinyl, on.vinyl }, { off.grid, on.grid } })
		EXPECT_EQ (a, b);

	EXPECT_LT (on.remaining.bottom(), on.gridNudge[0].y + 1) << "Grid Adjust between the times and the transport";
	EXPECT_LT (on.gridEdit[0].bottom(), on.cue.y + 1);
	EXPECT_EQ (off.loopOff.x, off.elapsed.x) << "the keys span the whole column";

	// With GRID off the times take the Grid Adjust rows' room: nothing empty.
	EXPECT_LE (off.elapsed.y, on.elapsed.y);
	EXPECT_GE (off.remaining.bottom(), on.gridEdit[0].bottom());
	EXPECT_GT (off.elapsed.h, on.elapsed.h) << "bigger when they have the room";
}

TEST (SurfaceLayout, DeckKeysAreAtLeastFingerHighOnTheRig)
{
	const auto s = surface::sections (rigWidth, rigHeight);
	const auto c = surface::deckColumn (Rect { 0, 0, s.deck[0].w, s.deck[0].h }, true);

	for (const auto& key : { c.cue, c.play, c.loopOff, c.sync, c.vinyl, c.grid })
		EXPECT_GE (key.h, 34);
	for (const auto& key : c.gridNudge)
		EXPECT_GE (key.h, 28);
	EXPECT_GE (surface::band (s.band).deck[0].range.h, 30);
}

// The mixer's stem rows: the knob half its old size, the bus switches and
// mute in one row, the four rows tight under each other -- a matrix of 4 x 6
// keys per deck, the two decks' matrices apart.
TEST (SurfaceLayout, StemRowsFormAMatrix)
{
	const auto s = surface::sections (rigWidth, rigHeight);
	const auto strips = surface::mixerStrips (Rect { 0, 0, s.mixer.w, s.mixer.h });
	const auto strip = surface::channelStrip (Rect { 0, 0, strips[0].w, strips[0].h });

	for (size_t r = 0; r < strip.stems.size(); ++r)
	{
		const auto& row = strip.stems[r];
		EXPECT_LE (row.knob.w, 28) << "half the 48 px knob";
		for (const auto& bus : row.buses)
		{
			EXPECT_EQ (bus.y, row.buses[0].y);
			EXPECT_EQ (bus.h, row.mute.h);
			EXPECT_GE (bus.h, 30) << "the keys grew into the freed height";
		}
		EXPECT_EQ (row.mute.y, row.buses[0].y);
		EXPECT_GT (row.mute.x, row.buses.back().right());
		EXPECT_TRUE (row.frame.contains (row.mute));

		if (r + 1 < strip.stems.size())
		{
			const auto& next = strip.stems[r + 1];
			EXPECT_EQ (row.frame.bottom(), next.frame.y) << "no gap between the rows";
			EXPECT_LE (next.buses[0].y - row.buses[0].bottom(), 2) << "keys a hairline apart";
			for (size_t k = 0; k < row.buses.size(); ++k)
				EXPECT_EQ (row.buses[k].x, next.buses[k].x) << "columns line up";
		}
	}
	EXPECT_GE (strips[1].x - strips[0].right(), 6) << "deck A's and deck B's matrices apart";
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
