#pragma once

// Peak-meter ballistics, one step per display tick: fast attack, slow release.
// Pure: takes the shown level and the peak since the last tick (linear gain),
// returns the level to show next.
float nextMeterLevel (float shownLevel, float newPeak);
