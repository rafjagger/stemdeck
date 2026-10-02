"""StemDeck's zita units on radla take their addresses from the one truth.

The j2n unit sent StemDeck's ten channels to 192.168.43.129 -- A3 Core's
address before the rig moved to 192.168.8.x -- and zita said nothing: UDP
does not know whether anybody listens. Since 2026-09-30 the units start zita
through tools/zita-from-truth.py, which reads a3-core's a3-osc.json.
"""

import importlib.util
import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("zita_from_truth", REPO / "tools/zita-from-truth.py")
zita = importlib.util.module_from_spec(spec)
spec.loader.exec_module(zita)

TRUTH = {
    "hosts": {"core": "10.9.9.10", "radla": "10.9.9.96", "local": "127.0.0.1", "any": "0.0.0.0"},
    "listeners": [
        {"program": "zita-n2j", "role": "audio", "host": "any", "port": 16510},
        {"program": "radla", "role": "zita-n2j", "host": "radla", "port": 15510},
    ],
}


class WhereZitaGoes(unittest.TestCase):
    def test_j2n_sends_to_cores_n2j_at_cores_address(self):
        # From radla, Core's "every interface" is Core's address.
        self.assertEqual(zita.zita_command(TRUTH, "j2n", ["--chan", "10", "--24bit"]),
                         ["zita-j2n", "--chan", "10", "--24bit", "10.9.9.10", "16510"])

    def test_n2j_listens_on_radlas_port_on_every_interface(self):
        self.assertEqual(zita.zita_command(TRUTH, "n2j", ["--chan", "1-2"]),
                         ["zita-n2j", "--chan", "1-2", "0.0.0.0", "15510"])

    def test_a_missing_listener_is_said(self):
        with self.assertRaises(SystemExit) as caught:
            zita.zita_command({"hosts": {}, "listeners": []}, "j2n", [])
        self.assertIn("zita-n2j", str(caught.exception))


class TheUnitsCarryNoAddress(unittest.TestCase):
    def test_both_units_start_zita_through_the_truth(self):
        for name in ("zita-j2n.service", "zita-n2j.service"):
            with self.subTest(unit=name):
                text = (REPO / ".config/systemd/user" / name).read_text()
                exec_start = [l for l in text.splitlines() if l.startswith("ExecStart=")][0]
                self.assertIn("zita-from-truth.py", exec_start)
                self.assertNotRegex(exec_start, r"\d+\.\d+\.\d+\.\d+")


class FindingTheTruth(unittest.TestCase):
    def test_the_environment_points_elsewhere(self):
        # Without it: TheCacheComesFirst, against a home of its own.
        saved = os.environ.get("A3_OSC_TRUTH")
        os.environ["A3_OSC_TRUTH"] = "/tmp/elsewhere.json"
        try:
            self.assertEqual(zita.truth_path(), Path("/tmp/elsewhere.json"))
        finally:
            del os.environ["A3_OSC_TRUTH"]
            if saved is not None:
                os.environ["A3_OSC_TRUTH"] = saved


class TheCacheComesFirst(unittest.TestCase):
    """On radla StemDeck fetches Core's truth into ~/.cache/a3 (step 4);
    zita reads what StemDeck fetched, so radla needs no copy in /usr/share."""

    def setUp(self):
        self.saved = os.environ.pop("A3_OSC_TRUTH", None)
        self.home = Path(tempfile.mkdtemp())
        self.cache = self.home / ".cache/a3/a3-osc.json"
        self.cache.parent.mkdir(parents=True)

    def tearDown(self):
        if self.saved is not None:
            os.environ["A3_OSC_TRUTH"] = self.saved
        shutil.rmtree(self.home)

    def test_a_cache_that_reads_is_taken(self):
        self.cache.write_text('{"listeners": []}')
        self.assertEqual(zita.truth_path(self.home), self.cache)

    def test_a_cache_that_does_not_read_is_skipped(self):
        self.cache.write_text("garbage")
        self.assertEqual(zita.truth_path(self.home), Path("/usr/share/a3/a3-osc.json"))

    def test_no_cache_is_the_package(self):
        self.assertEqual(zita.truth_path(self.home), Path("/usr/share/a3/a3-osc.json"))

if __name__ == "__main__":
    unittest.main()
