# SPDX-FileCopyrightText: 2026 Raphael Eismann
# SPDX-License-Identifier: GPL-3.0-or-later
"""StemDeck takes the engine's DSP sources from the a3-core repo (a3-engine is
archived, its history lives in a3-core). The submodule sits at third_party/a3-core."""

import subprocess
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
BUILD_FILES = [REPO / "CMakeLists.txt", *REPO.glob("tests/**/CMakeLists.txt"),
               *REPO.glob("tools/**/CMakeLists.txt")]


class EngineSourcesFromACore(unittest.TestCase):
    def test_submodule_is_a3_core(self):
        # --file reads .gitmodules from the tree, so no initialised submodule is needed.
        out = subprocess.run(["git", "-C", REPO, "config", "--file", ".gitmodules", "--get-regexp",
                              r"^submodule\.third_party/a3-core\.(path|url)$"],
                             capture_output=True, text=True).stdout
        self.assertIn("submodule.third_party/a3-core.path third_party/a3-core", out)
        self.assertIn("submodule.third_party/a3-core.url https://github.com/a3-audio/a3-core", out)

    def test_no_build_file_names_the_old_submodule(self):
        for path in BUILD_FILES:
            text = path.read_text()
            with self.subTest(file=str(path.relative_to(REPO))):
                self.assertNotIn("third_party/a3-engine", text)
                self.assertNotIn("STEMDECK_A3_ENGINE_DIR", text)


if __name__ == "__main__":
    unittest.main()
