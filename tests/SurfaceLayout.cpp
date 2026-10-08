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
			const auto origin = s.pitch[(size_t) d];
			const auto p = surface::pitchColumn (Rect { 0, 0, origin.w, origin.h });
			all.push_back ({ "waves", s.waves,
							 { { "pitch" + deckName (d), p.fader.translated (origin.x, origin.y), true },
							   { "range" + deckName (d), p.range.translated (origin.x, origin.y), true } } });
		}

		for (int d = 0; d < 2; ++d)
		{
			const auto origin = s.deck[(size_t) d];
			const auto c = surface::deckColumn (Rect { 0, 0, origin.w, origin.h }, gridOn[(size_t) d]);
			const auto at = [&origin] (const Rect& r) { return r.translated (origin.x, origin.y); };
			Section deck { "deck" + deckName (d), origin, {} };
			const auto add = [&] (const char* name, const Rect& r, bool interactive)
			{
				deck.controls.push_back ({ deck.name + "." + name, at (r), interactive });
			};
			add ("title", c.title, false);
			add ("stems", c.stems, false);
			add ("overview", c.overview, true);
			add ("elapsed", c.elapsed, false);
			add ("remaining", c.remaining, false);
			add ("bpm", c.bpm, false);
			add ("bpmInfo", c.bpmInfo, false);
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
					add (("nudge" + std::to_string (i)).c_str(), c.gridNudge[i], true);
				for (size_t i = 0; i < c.gridEdit.size(); ++i)
					add (("gridEdit" + std::to_string (i)).c_str(), c.gridEdit[i], true);
			}
			all.push_back (deck);
		}

		const auto m = surface::mixer (Rect { 0, 0, s.mixer.w, s.mixer.h });
		Section mixer { "mixer", s.mixer, {} };
		const auto inMixer = [&s] (const Rect& r) { return r.translated (s.mixer.x, s.mixer.y); };
		mixer.controls.push_back ({ "meters", inMixer (m.meters), false });

		for (int d = 0; d < 2; ++d)
		{
			const auto stripOrigin = m.strip[(size_t) d];
			const auto strip = surface::channelStrip (Rect { 0, 0, stripOrigin.w, stripOrigin.h }, d, m.meterReserve);
			const auto at = [&] (const Rect& r) { return inMixer (r.translated (stripOrigin.x, stripOrigin.y)); };
			const auto prefix = "strip" + deckName (d) + ".";

			for (size_t stem = 0; stem < strip.stems.size(); ++stem)
			{
				const auto& row = strip.stems[stem];
				const auto stemName = prefix + "stem" + std::to_string (stem) + ".";
				mixer.controls.push_back ({ stemName + "label", at (row.label), false });
				mixer.controls.push_back ({ stemName + "knob", at (row.knob), true });
				mixer.controls.push_back ({ stemName + "mute", at (row.mute), true });
				for (size_t bus = 0; bus < row.buses.size(); ++bus)
					mixer.controls.push_back ({ stemName + "bus" + std::to_string (bus), at (row.buses[bus]), true });
			}
			mixer.controls.push_back ({ prefix + "fader", at (strip.fader), true });
		}
		all.push_back (mixer);

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
		{ "mixer", s.mixer }, { "library", s.library } };

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

// The pitch faders stand at the outer edges of the waveform section, as tall
// as it: A left, B right, the waveforms between them.
TEST (SurfaceLayout, PitchFadersFlankTheWaveforms)
{
	const auto s = surface::sections (rigWidth, rigHeight);
	EXPECT_EQ (s.pitch[0].x, s.waves.x);
	EXPECT_EQ (s.pitch[1].right(), s.waves.right());
	EXPECT_EQ (s.pitch[0].h, s.waves.h);
	EXPECT_EQ (s.pitch[1].h, s.waves.h);
	EXPECT_LT (s.pitch[0].right(), s.waveA.x);
	EXPECT_GT (s.pitch[1].x, s.waveA.right());

	const auto p = surface::pitchColumn (Rect { 0, 0, s.pitch[0].w, s.pitch[0].h });
	EXPECT_LT (p.fader.bottom(), p.range.y + 1) << "the range key below the fader";
	EXPECT_GT (p.valueHeight, 0);
}

// The point of the relayout: a deck's keys span the whole column, and none
// moves when GRID turns on -- the finger that turned it on turns it off.
TEST (SurfaceLayout, DeckKeysUseTheWholeColumnAndStayPutWithGrid)
{
	const Rect column { 0, 0, 178, 460 };
	const auto off = surface::deckColumn (column, false);
	const auto on = surface::deckColumn (column, true);

	EXPECT_EQ (off.loopOff.x, off.overview.x);
	EXPECT_EQ (off.repeat.right(), off.overview.right());
	EXPECT_EQ (off.previous.x, off.overview.x);
	EXPECT_EQ (off.next.right(), off.overview.right());

	for (const auto& [a, b] : { std::pair { off.cue, on.cue }, { off.play, on.play }, { off.loopOff, on.loopOff },
								{ off.sync, on.sync }, { off.vinyl, on.vinyl }, { off.grid, on.grid } })
		EXPECT_EQ (a, b);

	EXPECT_LT (on.overview.h, off.overview.h) << "the Grid Adjust rows take their room from the overview";
	EXPECT_GE (on.overview.h, column.h / 8) << "but some overview is left";
}

TEST (SurfaceLayout, DeckKeysAreAtLeastFingerHighOnTheRig)
{
	const auto s = surface::sections (rigWidth, rigHeight);
	const auto c = surface::deckColumn (Rect { 0, 0, s.deck[0].w, s.deck[0].h }, true);

	for (const auto& key : { c.cue, c.play, c.loopOff, c.sync, c.vinyl, c.grid })
		EXPECT_GE (key.h, 36);
	for (const auto& key : c.gridNudge)
		EXPECT_GE (key.h, 30);
}

// The mixer's stem rows: the knob half its old size, the bus switches and
// mute in one row; the room they gave up goes to the faders.
TEST (SurfaceLayout, StemRowsAreOneLineOfSwitches)
{
	const auto s = surface::sections (rigWidth, rigHeight);
	const auto m = surface::mixer (Rect { 0, 0, s.mixer.w, s.mixer.h });
	const auto strip = surface::channelStrip (Rect { 0, 0, m.strip[0].w, m.strip[0].h }, 0, m.meterReserve);

	for (const auto& row : strip.stems)
	{
		EXPECT_LE (row.knob.w, 28) << "half the 48 px knob";
		for (const auto& bus : row.buses)
		{
			EXPECT_EQ (bus.y, row.buses[0].y);
			EXPECT_EQ (bus.h, row.mute.h);
		}
		EXPECT_EQ (row.mute.y, row.buses[0].y);
		EXPECT_GT (row.mute.x, row.buses.back().right());
	}
	EXPECT_GE (strip.fader.h, m.strip[0].h * 2 / 5) << "the faders got the room";
}

// Volume faders directly beside the output meters: A on their left, B on
// their right; and the meters slim.
TEST (SurfaceLayout, VolumeFadersFlankTheOutputMeters)
{
	const auto s = surface::sections (rigWidth, rigHeight);
	const auto m = surface::mixer (Rect { 0, 0, s.mixer.w, s.mixer.h });
	const auto stripA = surface::channelStrip (Rect { 0, 0, m.strip[0].w, m.strip[0].h }, 0, m.meterReserve);
	const auto stripB = surface::channelStrip (Rect { 0, 0, m.strip[1].w, m.strip[1].h }, 1, m.meterReserve);
	const auto faderA = stripA.fader.translated (m.strip[0].x, m.strip[0].y);
	const auto faderB = stripB.fader.translated (m.strip[1].x, m.strip[1].y);

	EXPECT_EQ (faderA.right(), m.meters.x);
	EXPECT_EQ (faderB.x, m.meters.right());
	EXPECT_GE (m.meters.w, 100);
	EXPECT_LE (m.meters.w, 120);
	EXPECT_EQ (faderA.y, m.meters.y) << "top-aligned with the meter bars";
	EXPECT_EQ (faderA.bottom(), m.meters.bottom() - surface::meterCaptionHeight (m.meters.h));
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
