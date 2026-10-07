"""The stemdeck package: its control file, its maintainer scripts, its tree.

The package writes only to /usr. postinst enables the user unit for every
user and does nothing else -- no write into any home, no restart (spec
app-packages, 2026-10-07). User data stays on purge."""

import os
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DEBIAN = REPO / "packaging/DEBIAN"
STAGE = REPO / "packaging/stage"
UNIT = REPO / ".config/systemd/user/stemdeck.service"
EXTRA_DEPENDS = ("jack-example-tools", "xdotool", "x11-xserver-utils", "x11-utils",
                 "python3", "onboard")
TOOLS = ("a3-wait-for-the-screen", "rig-keep-the-screen.sh", "setup-separator.sh",
         "zita-from-truth.py", "stemdeck-seed")


def fields(text):
    found = {}
    for line in text.splitlines():
        if line and not line[0].isspace() and ":" in line:
            key, _, value = line.partition(":")
            found[key] = value.strip()
    return found


def code_lines(path):
    """The lines that run: no comments, no echo (a printed hint is not an action)."""
    return [line for line in path.read_text().splitlines()
            if line.strip() and not line.strip().startswith(("#", "echo"))]


class TheControlFile(unittest.TestCase):
    def setUp(self):
        self.text = (DEBIAN / "control").read_text()
        self.fields = fields(self.text)

    def test_name_architecture_and_a_version_to_stamp(self):
        self.assertEqual("stemdeck", self.fields["Package"])
        self.assertEqual("amd64", self.fields["Architecture"])
        self.assertIn("Version", self.fields)

    def test_depends_are_the_binarys_libraries_and_the_units_tools(self):
        depends = [d.strip() for d in self.fields["Depends"].split(",")]
        self.assertEqual("${shlibs:Depends}", depends[0])
        for package in EXTRA_DEPENDS:
            self.assertIn(package, depends)

    def test_it_does_not_need_the_core(self):
        """a3nuc2 runs StemDeck without a Core and must not get REAPER and i3's config."""
        self.assertNotIn("a3-core", self.text)

    def test_it_says_user_data_stays(self):
        self.assertIn("User data stays", self.text)


class TheMaintainerScripts(unittest.TestCase):
    def test_they_are_executable(self):
        for name in ("postinst", "postrm"):
            with self.subTest(script=name):
                self.assertTrue(os.access(DEBIAN / name, os.X_OK))

    def test_postinst_enables_for_every_user(self):
        self.assertIn("systemctl --global enable stemdeck.service", (DEBIAN / "postinst").read_text())

    def test_postinst_writes_into_no_home_and_restarts_nothing(self):
        for line in code_lines(DEBIAN / "postinst"):
            for forbidden in ("restart", "/home", "$HOME", "sudo", "--user enable",
                              "--user start", "cp ", "mv ", "mkdir", "rm ", "chown"):
                with self.subTest(line=line, forbidden=forbidden):
                    self.assertNotIn(forbidden, line)

    def test_postrm_disables_on_purge_and_removes_nothing(self):
        text = (DEBIAN / "postrm").read_text()
        self.assertIn("purge", text)
        self.assertIn("systemctl --global disable stemdeck.service", text)
        for line in code_lines(DEBIAN / "postrm"):
            self.assertNotIn("rm ", line)


class TheStagedTree(unittest.TestCase):
    EXPECTED = {
        "usr/bin/stemdeck",
        *(f"usr/lib/stemdeck/{tool}" for tool in TOOLS),
        "usr/share/stemdeck/stemdeck-without-core.xml",
        "usr/lib/systemd/user/stemdeck.service",
        "usr/share/doc/stemdeck/copyright",
    }

    def stage(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        build = Path(tmp.name) / "build"
        binary = build / "StemDeck_artefacts/Release/StemDeck"
        binary.parent.mkdir(parents=True)
        binary.write_bytes(b"\x7fELF not really")
        stage = Path(tmp.name) / "stage"
        stage.mkdir()
        subprocess.run([STAGE, "files", REPO, build, stage], check=True)
        return stage

    def files(self, stage):
        return {str(p.relative_to(stage)) for p in stage.rglob("*") if p.is_file()}

    def test_exactly_these_files(self):
        self.assertEqual(self.EXPECTED, self.files(self.stage()))

    def test_programs_are_executable_and_data_is_not(self):
        stage = self.stage()
        for name in self.files(stage):
            mode = (stage / name).stat().st_mode & 0o777
            expected = 0o755 if name.startswith(("usr/bin/", "usr/lib/stemdeck/")) else 0o644
            with self.subTest(file=name):
                self.assertEqual(expected, mode)

    def test_the_unit_is_the_repositorys(self):
        stage = self.stage()
        self.assertEqual(UNIT.read_bytes(),
                         (stage / "usr/lib/systemd/user/stemdeck.service").read_bytes())

    def test_every_program_the_unit_starts_is_in_the_tree(self):
        stage = self.stage()
        for line in UNIT.read_text().splitlines():
            key, _, value = line.partition("=")
            if not key.startswith("Exec"):
                continue
            for word in value.lstrip("-@:+!").split():
                if word.startswith(("/usr/lib/stemdeck/", "/usr/bin/stemdeck")):
                    with self.subTest(program=word):
                        self.assertTrue((stage / word.lstrip("/")).is_file())

    def test_an_unknown_step_is_refused(self):
        done = subprocess.run([STAGE, "nonsense"], capture_output=True)
        self.assertEqual(2, done.returncode)


if __name__ == "__main__":
    unittest.main()
