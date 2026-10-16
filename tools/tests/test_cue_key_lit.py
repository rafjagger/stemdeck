"""The on-screen CUE key shows CUE, whoever pressed it (keyboard, SCS.3d,
panel all call DeckPanel::cuePressed()): it is lit exactly while CUE is held.

DeckPanel is JUCE and not in the test runner, so the wiring is read from the
source."""

import re
import unittest
from pathlib import Path

SOURCE = Path(__file__).resolve().parents[2] / "Source"
TEXT = (SOURCE / "DeckPanel.cpp").read_text()


def body(name):
    match = re.search(rf"void DeckPanel::{name}\s*\(\)\s*\{{(.*?)\n\}}", TEXT, re.S)
    assert match, name
    return match.group(1)


LIGHT = r"cueButton\.setToggleState\s*\(\s*{}\s*,\s*juce::dontSendNotification\s*\)"


class CueKeyIsLitWhileHeld(unittest.TestCase):
    def test_press_lights_the_key_before_any_early_return(self):
        pressed = body("cuePressed")
        lit = re.search(LIGHT.format("true"), pressed)
        self.assertTrue(lit, "cuePressed() must light the key")
        self.assertLess(lit.start(), pressed.index("return"),
                        "an unloaded deck still shows the key held")

    def test_release_darkens_the_key_before_any_condition(self):
        released = body("cueReleased")
        dark = re.search(LIGHT.format("false"), released)
        self.assertTrue(dark, "cueReleased() must darken the key")
        self.assertLess(dark.start(), released.index("if"))

    def test_lit_colours_are_readable(self):
        self.assertIn("cueButton.setColour (juce::TextButton::buttonOnColourId, Theme::cue)", TEXT)
        self.assertRegex(TEXT, r"cueButton\.setColour \(juce::TextButton::textColourOnId, [^)]+\)")

    def test_the_screen_key_still_calls_press_once(self):
        wiring = TEXT[TEXT.index("cueButton.onStateChange"):TEXT.index("cueButton.setColour")]
        self.assertEqual(wiring.count("cuePressed()"), 1)
        self.assertEqual(wiring.count("cueReleased()"), 1)
        self.assertIn("cueButtonWasDown", wiring)


if __name__ == "__main__":
    unittest.main()
