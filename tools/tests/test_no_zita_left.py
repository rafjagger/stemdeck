"""zita is gone from the whole system (2026-10-14): PipeWire is the only audio
server, and a remote StemDeck reaches the Core over AES67. Nothing in the tree
may start, ship or name it, apart from the tests that say so."""

import subprocess
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SELF = Path(__file__).resolve()
ALLOWED = {SELF, *(REPO / "tools/tests" / n for n in (
    "test_patchbay_without_core.py", "test_stemdeck_needs_jack.py", "test_package_tree.py"))}


class NoZitaLeft(unittest.TestCase):
    def test_no_tracked_file_names_zita(self):
        listed = subprocess.run(["git", "-C", REPO, "ls-files", "-z"], capture_output=True,
                                check=True).stdout.decode().split("\0")
        for name in filter(None, listed):
            path = REPO / name
            if path in ALLOWED or path.name == ".git" or not path.is_file():
                continue
            with self.subTest(file=name):
                self.assertNotIn("zita", name.lower())
                self.assertNotIn("zita", path.read_bytes().decode(errors="ignore").lower())


if __name__ == "__main__":
    unittest.main()
