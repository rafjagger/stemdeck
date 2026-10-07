"""The test objects are tied to the installed googletest (stemdeck#1).

Debian keeps upstream file times, so after a libgtest-dev upgrade make sees
nothing newer than the objects and links old objects against the new library.
tests/gtest-stamp.cmake writes a header from the library's content; the test
objects include it, so a different googletest means a different header and a
rebuild."""

import subprocess
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SCRIPT = REPO / "tests/gtest-stamp.cmake"


def stamp(header, *files):
    args = ["cmake", f"-DOUT={header}", f"-DFILES={';'.join(map(str, files))}",
            "-P", str(SCRIPT)]
    subprocess.run(args, check=True, capture_output=True)


class GtestStampTest(unittest.TestCase):
    def setUp(self):
        self.dir = Path(tempfile.mkdtemp())
        self.lib = self.dir / "libgtest.a"
        self.lib.write_bytes(b"gtest 1.17")
        self.header = self.dir / "gtest_stamp.h"

    def test_new_library_changes_the_header(self):
        stamp(self.header, self.lib)
        before = self.header.read_text()
        self.lib.write_bytes(b"gtest 1.18")
        stamp(self.header, self.lib)
        self.assertNotEqual(before, self.header.read_text())

    def test_same_library_leaves_the_header_untouched(self):
        stamp(self.header, self.lib)
        first = self.header.stat().st_mtime_ns
        stamp(self.header, self.lib)
        self.assertEqual(first, self.header.stat().st_mtime_ns)

    def test_test_objects_include_the_stamp(self):
        cmake = (REPO / "tests/CMakeLists.txt").read_text()
        self.assertIn("gtest-stamp.cmake", cmake)
        self.assertIn("-include", cmake)


if __name__ == "__main__":
    unittest.main()
