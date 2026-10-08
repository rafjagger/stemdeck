"""StemDeck starts only once JACK runs, and stands on its own.

After a reboot JACK failed to start (the sound card was late); StemDeck,
only ordered after it, started without JACK anyway, was stopped, and stayed
down when JACK came up later (2026-10-02). It was bound to a3-jack for that.

But a3-jack exists only on a Core, and StemDeck may run on a machine of its
own: a unit bound to a missing unit is never started ("Unit a3-jack.service
not found", 2026-10-05). So the unit itself waits for whatever JACK runs
(jack_wait) and uses only files the stemdeck package ships; the binding to a3-jack
comes from the Core, as a drop-in the a3-core package ships."""

import configparser
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
UNIT = REPO / ".config/systemd/user/stemdeck.service"


def section(name):
    parser = configparser.ConfigParser(strict=False, interpolation=None)
    parser.optionxform = str
    parser.read_string(UNIT.read_text())
    return parser[name]


def exec_lines():
    return [line.split("=", 1)[1] for line in UNIT.read_text().splitlines()
            if line.startswith(("ExecStartPre=", "ExecStart=", "ExecStartPost="))]


class StemDeckNeedsJack(unittest.TestCase):
    def test_it_waits_for_jack(self):
        self.assertIn("/usr/bin/jack_wait -w", exec_lines())

    def test_a_jack_start_brings_it_up(self):
        """On a Core every a3-jack start pulls it up (after re-enabling); on
        a machine without a3-jack the entry is a harmless link."""
        wanted = section("Install").get("WantedBy", "").split()
        self.assertIn("a3-jack.service", wanted)
        self.assertIn("default.target", wanted)


class StemDeckStandsAlone(unittest.TestCase):
    def test_it_needs_no_unit_of_the_core(self):
        for key in ("BindsTo", "Requires", "Requisite"):
            self.assertNotIn("a3-", section("Unit").get(key, ""), key)

    def test_it_runs_only_packaged_files_and_the_system_s(self):
        for line in exec_lines():
            program = line.lstrip("-").split()[0]
            self.assertTrue(program.startswith(("/usr/", "/bin/")), line)

    def test_its_screen_wait_is_the_packaged_copy(self):
        wait = REPO / "tools/a3-wait-for-the-screen"
        self.assertIn("/usr/lib/stemdeck/a3-wait-for-the-screen", exec_lines())
        self.assertTrue(wait.is_file())
        self.assertTrue(wait.stat().st_mode & 0o111, "executable")


if __name__ == "__main__":
    unittest.main()
