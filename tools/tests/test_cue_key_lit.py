"""The on-screen CUE key lights like a CDJ's CUE (CueLight.h, unit-tested),
whoever pressed CUE (keyboard, SCS.3d, panel all call DeckPanel::cuePressed()).

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


class CueKeyFollowsTheLight(unittest.TestCase):
    def test_press_holds_and_updates_before_any_early_return(self):
        pressed = body("cuePressed")
        self.assertLess(pressed.index("cueHeld = true"), pressed.index("updateCueLight"))
        self.assertLess(pressed.index("updateCueLight"), pressed.index("return"))

    def test_release_lets_go_and_updates_before_any_condition(self):
        released = body("cueReleased")
        self.assertLess(released.index("cueHeld = false"), released.index("updateCueLight"))
        self.assertLess(released.index("updateCueLight"), released.index("if"))

    def test_the_light_is_set_without_notification(self):
        light = body("updateCueLight")
        self.assertRegex(light, r"cueButton\.setToggleState \(lit, juce::dontSendNotification\)")

    def test_the_ui_refresh_drives_it(self):
        self.assertIn("updateCueLight();", body("refresh"))

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
