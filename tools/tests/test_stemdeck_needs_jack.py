"""StemDeck starts only once the audio graph runs, and stands on its own.

After a reboot the audio server failed to start (the sound card was late);
StemDeck, only ordered after it, started without it anyway, was stopped, and
stayed down when it came up later (2026-10-02).

The audio server is PipeWire (its JACK API is system-wide); there is no
jackd. A unit bound to a unit of the Core that is missing is never started
(2026-10-05), so StemDeck is ordered after PipeWire and waits for the graph
(jack_wait) and uses only files the stemdeck package ships; the binding
comes from the Core, as a drop-in the a3-core package ships."""

import configparser
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
UNIT = REPO / ".config/systemd/user/stemdeck.service"
UNITS = sorted(UNIT.parent.glob("*.service"))


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

    def test_it_is_ordered_after_pipewire(self):
        after = section("Unit").get("After", "").split()
        self.assertIn("pipewire.service", after)
        self.assertIn("wireplumber.service", after)

    def test_it_is_wanted_by_the_session_only(self):
        self.assertEqual(["default.target"], section("Install").get("WantedBy", "").split())

    def test_no_unit_names_jackd_or_a_jack_unit(self):
        for unit in UNITS:
            text = "\n".join(l for l in unit.read_text().splitlines() if not l.startswith("#"))
            for forbidden in ("a3-jack", "jackd", "zita", "LD_LIBRARY_PATH"):
                with self.subTest(unit=unit.name, forbidden=forbidden):
                    self.assertNotIn(forbidden, text)

    def test_qjackctl_is_ordered_after_pipewire_too(self):
        parser = configparser.ConfigParser(strict=False, interpolation=None)
        parser.optionxform = str
        parser.read_string((UNIT.parent / "qjackctl-stemdeck.service").read_text())
        after = parser["Unit"].get("After", "").split()
        self.assertIn("pipewire.service", after)
        self.assertIn("wireplumber.service", after)


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
