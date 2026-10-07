#pragma once

// How a meter moves (decided 2026-10-07, one behaviour across the system):
// the sources send raw peaks, every display applies these ballistics on its
// own clock. Core's truth carries the numbers ("meters"); these are the
// defaults when it does not.
struct MeterParameters
{
	float attackMs = 0.0f;            // 0: a higher peak shows at once
	float releaseDbPerSecond = 20.0f; // the bar's fall, and the hold's after its time
	float peakHoldSeconds = 1.5f;     // how long the highest level stays marked
};

// A peak meter's bar and its hold mark. Pure: fed once per display tick with
// the peak since the last tick (linear gain) and the seconds since then.
class PeakMeter
{
public:
	static constexpr float floorDb = -60.0f; // the bottom of the scale: below it, nothing

	explicit PeakMeter (MeterParameters parameters = {}) : parameters (parameters) {}
	void setParameters (MeterParameters newParameters) { parameters = newParameters; }

	void feed (float peak, float seconds);

	// dBFS, floorDb when silent. Mapped onto a bar by barFraction.
	float levelDb() const { return shownDb; }
	float holdDb() const { return heldDb; }

private:
	float risenTowards (float peakDb, float seconds) const;

	MeterParameters parameters;
	float shownDb = floorDb;
	float heldDb = floorDb;
	float holdRemaining = 0.0f;
};

// The desk's LED scale (a3-mixer a3_mixer_meters.py, decided 2026-10-07), so
// a StemDeck bar stands where the desk's display bar and channel LEDs do: LED
// threshold k (-36 -24 -18 -12 -9 -6 -3 0 dBFS) at k/8 of the bar, linear in
// dB between them; below -36 on at the first step's slope to an empty bar at
// meterScaleFloorDb; 0 dBFS and over full.
inline constexpr float meterScaleFloorDb = -48.0f;
float barFraction (float db);

// How many of a bar's segments a level lights: a segment once the level
// reaches its top, as an LED at its threshold. The bar and its hold mark use
// the same count, so the mark sits where the bar was.
int segmentsLit (float db, int segments);

// A segment's colour, the desk's LEDs': green up to the -12 LED, yellow at
// -9 and -6, red at -3 and 0 -- by which LED's eighth the segment's top is in.
enum class MeterZone { green, yellow, red };
MeterZone zoneOfSegment (int index, int segments);

// How many LED segments a meter of this height draws: 24 where there is room,
// fewer where there is not -- each needs 3 px, a 2 px light and a 1 px gap.
// The REC meters in the 24 px top bar drew 24 segments of 0 px, nothing.
int meterSegments (float heightPixels);

// A clip lamp: lit by a peak above full scale (linear 1.0), held about a
// second after the last one. Pure: fed once per display tick with the peak
// since the last tick and the seconds since then.
class ClipHold
{
public:
	bool feed (float peak, float seconds);

private:
	float remaining = 0.0f;
};
