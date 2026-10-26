// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The bottom of a stem knob and of the channel fader: everything at or below
// it is silence. One value for the mixer and for whatever must agree with it.
constexpr float stemKnobMinimumDb = -60.0f;
