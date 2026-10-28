# SPDX-FileCopyrightText: 2026 Raphael Eismann
# SPDX-License-Identifier: GPL-3.0-or-later
"""The whole build rounds as x86-64 does: no fused multiply-add contraction.

GCC on aarch64 contracts a*b+c into one fmadd by default; x86-64's baseline has
no FMA, so the NUCs never did. On a Raspberry Pi 5 IEM's StereoEncoder then
computed a quaternion a rounding step away from the one it had stored, the
parameter's listeners ran on the audio thread and allocated there
(DeckSpatialAllocation.*, DeckSpatialMotion.APressDoesNotAllocateOnTheAudioThread
failed on arm64 only, 2026-10-28). The arm64 failure cannot show on the NUCs,
so this checks the setting that prevents it."""

import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]


class FloatContraction(unittest.TestCase):
    def test_contraction_is_off_before_any_target(self):
        text = (REPO / "CMakeLists.txt").read_text()
        code = "\n".join(line.split("#", 1)[0] for line in text.splitlines())
        flag = re.search(r"add_compile_options\s*\(\s*-ffp-contract=off\s*\)", code)
        self.assertIsNotNone(flag, "CMakeLists.txt sets no add_compile_options(-ffp-contract=off)")
        first_target = re.search(r"\b(add_library|add_executable|juce_add_gui_app)\s*\(", code)
        self.assertLess(flag.start(), first_target.start(),
                        "set before the first target, so JUCE, IEM and the app all get it")


if __name__ == "__main__":
    unittest.main()
