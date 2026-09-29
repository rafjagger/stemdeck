#pragma once

// Peak-meter ballistics, one step per display tick: fast attack, slow release.
// Pure: takes the shown level and the peak since the last tick (linear gain),
// returns the level to show next.
float nextMeterLevel (float shownLevel, float newPeak);

// How many LED segments a meter of this height draws: 24 where there is room,
// fewer where there is not -- each needs 3 px, a 2 px light and a 1 px gap.
// The REC meters in the 24 px top bar drew 24 segments of 0 px, nothing.
int meterSegments (float heightPixels);
