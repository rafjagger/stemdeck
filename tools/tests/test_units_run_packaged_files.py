"""StemDeck's units start only what the package ships or the system has.

The rig's unit ran the linker's output inside the checkout, and an in-place
rebuild left it with nothing to run: 203/EXEC, eight times (2026-10-07). The
package puts the binary in /usr/bin and the tools in /usr/lib/stemdeck; no
unit names a checkout any more."""

import subprocess
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
UNITS = REPO / ".config/systemd/user"
PACKAGED_TOOLS = "/usr/lib/stemdeck/"
SHIPPED_DATA = "/usr/share/stemdeck/"
EXEC_KEYS = ("ExecStartPre", "ExecStart", "ExecStartPost", "ExecStop", "ExecStopPost")


def command_lines(unit):
    """Each Exec line's words, its prefix (-, @, :, +, !) taken off."""
    for line in unit.read_text().splitlines():
        key, _, value = line.partition("=")
        if key in EXEC_KEYS:
            yield value.lstrip("-@:+!").split()


def every_unit():
    return sorted(UNITS.glob("*.service"))


class EveryUnit(unittest.TestCase):
    def test_nothing_runs_from_a_checkout(self):
        for unit in every_unit():
            with self.subTest(unit=unit.name):
                self.assertNotIn("a3-system", unit.read_text())

    def test_every_program_is_the_systems_or_the_packages(self):
        for unit in every_unit():
            for words in command_lines(unit):
                with self.subTest(unit=unit.name, line=words):
                    self.assertTrue(words[0].startswith(("/usr/bin/", PACKAGED_TOOLS)))

    def test_every_packaged_tool_is_in_tools(self):
        for unit in every_unit():
            for words in command_lines(unit):
                for word in words:
                    if word.startswith(PACKAGED_TOOLS):
                        with self.subTest(unit=unit.name, tool=word):
                            self.assertTrue((REPO / "tools" / word[len(PACKAGED_TOOLS):]).is_file())

    def test_every_shipped_file_is_in_the_repository(self):
        for unit in every_unit():
            for words in command_lines(unit):
                for word in words:
                    if word.startswith(SHIPPED_DATA):
                        with self.subTest(unit=unit.name, data=word):
                            self.assertTrue((REPO / ".config/rncbc.org" / word[len(SHIPPED_DATA):]).is_file())


class TheStemDeckUnit(unittest.TestCase):
    UNIT = UNITS / "stemdeck.service"

    def lines(self):
        return self.UNIT.read_text().splitlines()

    def test_it_starts_the_packaged_binary(self):
        self.assertIn("ExecStart=/usr/bin/stemdeck", self.lines())

    def test_it_runs_in_its_data_folder(self):
        """'-': on the very first start the folder is made by the seed."""
        self.assertIn("WorkingDirectory=-%h/.local/share/stemdeck", self.lines())

    def test_the_seed_runs_first_and_cannot_stop_the_start(self):
        pre = [line for line in self.lines() if line.startswith("ExecStartPre=")]
        self.assertEqual("ExecStartPre=-/usr/lib/stemdeck/stemdeck-seed", pre[0])

    def test_it_comes_back_after_a_crash(self):
        self.assertIn("Restart=on-failure", self.lines())


class TheTemplateUnit(unittest.TestCase):
    """stemdeck@.service: one one-deck instance per letter (S4.8)."""
    TEMPLATE = UNITS / "stemdeck@.service"
    TWO_DECK = UNITS / "stemdeck.service"

    def lines(self, unit):
        return unit.read_text().splitlines()

    def pre(self, unit):
        return [l for l in self.lines(unit) if l.startswith("ExecStartPre=")]

    def test_template_unit_starts_one_deck(self):
        self.assertIn("ExecStart=/usr/bin/stemdeck -bformat --deck %i --fullscreen",
                      self.lines(self.TEMPLATE))
        self.assertEqual(self.pre(self.TWO_DECK), self.pre(self.TEMPLATE))

    def test_template_conflicts_with_the_two_deck_unit(self):
        self.assertIn("Conflicts=stemdeck.service", self.lines(self.TEMPLATE))

    def test_template_is_never_enabled_by_itself(self):
        self.assertNotIn("[Install]", self.TEMPLATE.read_text())

    def test_template_keeps_the_two_deck_unit_s_environment(self):
        for line in ("Environment=DISPLAY=:0", "WorkingDirectory=-%h/.local/share/stemdeck",
                     "Restart=on-failure", "RestartSec=5",
                     "ExecStartPost=/usr/lib/stemdeck/rig-keep-the-screen.sh restore"):
            with self.subTest(line=line):
                self.assertIn(line, self.lines(self.TEMPLATE))

    def test_two_deck_unit_unchanged(self):
        committed = subprocess.run(
            ["git", "show", "main:.config/systemd/user/stemdeck.service"],
            cwd=REPO, capture_output=True, check=True).stdout
        self.assertEqual(committed, self.TWO_DECK.read_bytes())


if __name__ == "__main__":
    unittest.main()
