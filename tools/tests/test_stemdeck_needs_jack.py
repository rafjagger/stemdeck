"""StemDeck starts only once JACK runs, and goes and comes with it
(2026-10-02).

After a reboot JACK failed to start (the sound card was late); StemDeck,
only ordered after it, started without JACK anyway, was stopped, and stayed
down when JACK came up later. Bound to a3-jack, it waits for JACK and is
restarted with it."""

import configparser
import unittest
from pathlib import Path

UNIT = Path(__file__).resolve().parents[2] / ".config/systemd/user/stemdeck.service"


def section(name):
    parser = configparser.ConfigParser(strict=False, interpolation=None)
    parser.optionxform = str
    parser.read_string(UNIT.read_text())
    return parser[name]


def unit():
    return section("Unit")


class StemDeckNeedsJack(unittest.TestCase):
    def test_it_is_bound_to_jack(self):
        self.assertIn("a3-jack.service", unit().get("BindsTo", "").split())

    def test_it_starts_after_jack(self):
        self.assertIn("a3-jack.service", unit().get("After", "").split())

    def test_a_jack_start_brings_it_up(self):
        """BindsTo takes StemDeck down with JACK but never starts it again:
        wanted by a3-jack, every JACK start pulls it up (after re-enabling)."""
        wanted = section("Install").get("WantedBy", "").split()
        self.assertIn("a3-jack.service", wanted)
        self.assertIn("default.target", wanted)


if __name__ == "__main__":
    unittest.main()
