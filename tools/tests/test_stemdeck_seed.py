"""stemdeck-seed: StemDeck's data folder, and the recordings copied once out
of the checkout.

Until the package StemDeck recorded to ./recordings in its working directory,
the checkout: 415 MB in ~/a3-system/stemdeck/recordings on a3nuc1
(2026-10-07). The package records to ~/.local/share/stemdeck/recordings
(Source/DataPaths.cpp); the seed copies the old takes there at the first
start, as the user, and never moves or deletes one."""

import importlib.machinery
import importlib.util
import os
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
_loader = importlib.machinery.SourceFileLoader("stemdeck_seed", str(REPO / "tools/stemdeck-seed"))
_spec = importlib.util.spec_from_loader("stemdeck_seed", _loader)
seed = importlib.util.module_from_spec(_spec)
_loader.exec_module(seed)

PLENTY = lambda _path: 10 ** 12  # noqa: E731
NO_ROOM = lambda _path: 0  # noqa: E731


class SeedCase(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.home = Path(tmp.name)
        self.old = self.home / "a3-system/stemdeck/recordings"
        self.new = self.home / ".local/share/stemdeck/recordings"

    def take(self, name, data):
        self.old.mkdir(parents=True, exist_ok=True)
        path = self.old / name
        path.write_bytes(data)
        return path

    def run_seed(self, environ=None, free=PLENTY):
        log = []
        code = seed.seed(self.home, environ or {}, log.append, free)
        return code, log


class TheDataFolder(SeedCase):
    def test_the_recordings_folder_is_made(self):
        code, _ = self.run_seed()
        self.assertEqual(0, code)
        self.assertTrue(self.new.is_dir())

    def test_an_absolute_xdg_data_home_is_used(self):
        data = self.home / "xdg"
        self.run_seed({"XDG_DATA_HOME": str(data)})
        self.assertTrue((data / "stemdeck/recordings").is_dir())

    def test_a_relative_xdg_data_home_is_ignored(self):
        self.run_seed({"XDG_DATA_HOME": "data"})
        self.assertTrue(self.new.is_dir())

    def test_it_is_the_folder_stemdeck_records_to(self):
        source = (REPO / "Source/DataPaths.cpp").read_text()
        for literal in ('"/.local/share"', '"/stemdeck"', '"/recordings"'):
            self.assertIn(literal, source)
        self.assertEqual("stemdeck", seed.DATA_SUBDIR)
        self.assertEqual("recordings", seed.RECORDINGS)


class TheOldRecordings(SeedCase):
    def test_every_take_is_copied_byte_for_byte_and_keeps_its_time(self):
        first = self.take("StemDeck 2026-09-30 01-48-16.flac", b"one")
        os.utime(first, (1_700_000_000, 1_700_000_000))
        self.take("StemDeck 2026-09-30 20-30-10.flac", b"two")
        self.run_seed()
        for old in self.old.iterdir():
            new = self.new / old.name
            self.assertEqual(old.read_bytes(), new.read_bytes())
            self.assertEqual(int(old.stat().st_mtime), int(new.stat().st_mtime))

    def test_the_originals_stay(self):
        self.take("a.flac", b"one")
        self.run_seed()
        self.assertEqual(b"one", (self.old / "a.flac").read_bytes())

    def test_a_second_start_copies_nothing(self):
        self.take("a.flac", b"one")
        self.run_seed()
        os.utime(self.new / "a.flac", (1, 1))
        self.take("late.flac", b"late")
        _, log = self.run_seed()
        self.assertEqual(1, int((self.new / "a.flac").stat().st_mtime))
        self.assertFalse((self.new / "late.flac").exists(), "copied once, by design")
        self.assertEqual([], [line for line in log if "copied" in line])

    def test_a_copy_cut_off_is_finished_on_the_next_start(self):
        self.take("a.flac", b"complete take")
        self.new.mkdir(parents=True)
        (self.new / ".a.flac.part").write_bytes(b"compl")
        self.run_seed()
        self.assertEqual(b"complete take", (self.new / "a.flac").read_bytes())
        self.assertFalse((self.new / ".a.flac.part").exists())

    def test_a_different_file_of_that_name_is_kept_and_the_take_lands_beside_it(self):
        self.take("a.flac", b"from the checkout")
        self.new.mkdir(parents=True)
        (self.new / "a.flac").write_bytes(b"newer take")
        self.run_seed()
        self.assertEqual(b"newer take", (self.new / "a.flac").read_bytes())
        self.assertEqual(b"from the checkout", (self.new / "a (checkout).flac").read_bytes())

    def test_without_room_nothing_is_copied_and_the_next_start_tries_again(self):
        self.take("a.flac", b"one")
        code, log = self.run_seed(free=NO_ROOM)
        self.assertEqual(0, code)
        self.assertFalse((self.new / "a.flac").exists())
        self.assertTrue(any("not enough space" in line for line in log), log)
        self.run_seed()
        self.assertEqual(b"one", (self.new / "a.flac").read_bytes())

    def test_an_unreadable_checkout_is_logged_and_retried_and_the_start_goes_on(self):
        if os.geteuid() == 0:
            self.skipTest("root reads everything")
        self.take("a.flac", b"one")
        self.old.chmod(0)
        self.addCleanup(self.old.chmod, 0o755)
        code, log = self.run_seed()
        self.assertEqual(0, code)
        self.assertTrue(log)
        self.assertFalse((self.new.parent / seed.MARKER).exists())
        self.old.chmod(0o755)
        self.run_seed()
        self.assertEqual(b"one", (self.new / "a.flac").read_bytes())

    def test_no_checkout_no_marker(self):
        self.run_seed()
        self.assertFalse((self.new.parent / seed.MARKER).exists())


class TheHardCases(SeedCase):
    def test_a_dangling_symlink_is_skipped_and_the_good_takes_are_copied(self):
        self.take("a.flac", b"one")
        (self.old / "gone.flac").symlink_to(self.home / "nowhere")
        code, log = self.run_seed()
        self.assertEqual(0, code)
        self.assertEqual(b"one", (self.new / "a.flac").read_bytes())
        self.assertFalse((self.new / "gone.flac").exists())
        self.assertTrue(any("skipped" in line and "gone.flac" in line for line in log), log)
        self.assertTrue((self.new.parent / seed.MARKER).exists(), "skips do not block the marker")

    def test_a_symlinked_folder_is_logged_and_not_followed(self):
        self.take("a.flac", b"one")
        outside = self.home / "outside"
        outside.mkdir()
        (outside / "x.flac").write_bytes(b"x")
        (self.old / "link").symlink_to(outside)
        _, log = self.run_seed()
        self.assertFalse((self.new / "link").exists())
        self.assertTrue(any("skipped" in line and "link" in line for line in log), log)

    def test_files_in_subfolders_keep_their_relative_path(self):
        self.take("a.flac", b"one")
        (self.old / "sub/deeper").mkdir(parents=True)
        (self.old / "sub/deeper/b.flac").write_bytes(b"two")
        self.run_seed()
        self.assertEqual(b"two", (self.new / "sub/deeper/b.flac").read_bytes())

    def test_a_corrupt_marker_does_not_stop_the_start(self):
        self.take("a.flac", b"one")
        self.new.mkdir(parents=True)
        (self.new.parent / seed.MARKER).write_bytes(b"\xff\xfe\x80")
        code, _ = self.run_seed()
        self.assertEqual(0, code)
        self.assertEqual(b"one", (self.new / "a.flac").read_bytes())

    def test_an_error_that_is_not_an_oserror_still_exits_0(self):
        self.take("a.flac", b"one")
        def broken(_path):
            raise RuntimeError("boom")
        code, log = self.run_seed(free=broken)
        self.assertEqual(0, code)
        self.assertTrue(any("boom" in line for line in log), log)

    def test_main_exits_0_when_there_is_no_home(self):
        from unittest import mock
        with mock.patch.object(seed.Path, "home", side_effect=RuntimeError("no home")):
            self.assertEqual(0, seed.main())

    def test_undecodable_file_names_do_not_break_a_strict_log(self):
        self.old.mkdir(parents=True)
        (self.old / os.fsdecode(b"\xff.flac")).write_bytes(b"one")
        (self.old / os.fsdecode(b"\xfe.flac")).symlink_to(self.home / "nowhere")
        log = []
        code = seed.seed(self.home, {}, lambda line: log.append(line.encode("utf-8")), PLENTY)
        self.assertEqual(0, code)
        self.assertEqual(b"one", (self.new / os.fsdecode(b"\xff.flac")).read_bytes())
        self.assertTrue((self.new.parent / seed.MARKER).exists())

    def test_copies_and_the_marker_are_synced_to_disk(self):
        from unittest import mock
        self.take("a.flac", b"one")
        with mock.patch.object(seed.os, "fsync") as fsync:
            self.run_seed()
        self.assertGreaterEqual(fsync.call_count, 2)

    def test_names_that_collide_overwrite_nothing(self):
        self.take("a.flac", b"from the checkout")
        self.take("a (checkout).flac", b"checkout sibling")
        self.new.mkdir(parents=True)
        (self.new / "a.flac").write_bytes(b"newer take")
        self.run_seed()
        found = {p.read_bytes() for p in self.new.iterdir() if not p.name.startswith(".")}
        self.assertEqual({b"newer take", b"from the checkout", b"checkout sibling"}, found)
        self.assertEqual(b"newer take", (self.new / "a.flac").read_bytes())
        self.assertEqual(3, len([p for p in self.new.iterdir() if p.suffix == ".flac"]))


if __name__ == "__main__":
    unittest.main()
