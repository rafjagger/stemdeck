"""StemDeck's units start only what the package ships or the system has.

The rig's unit ran the linker's output inside the checkout, and an in-place
rebuild left it with nothing to run: 203/EXEC, eight times (2026-10-07). The
package puts the binary in /usr/bin and the tools in /usr/lib/stemdeck; no
unit names a checkout any more."""

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


if __name__ == "__main__":
    unittest.main()
