"""zita leaves real-time to JACK's process thread.

Both zita units raised the whole process to SCHED_RR 64, above JACK's own
process threads, and JACK reported late clients until it was gone (a3nuc1,
2026-10-06). zita's audio runs in its JACK process thread, which JACK raises
itself; nothing else in it needs to be real-time.

Not even as a comment: a commented-out line is the one that gets switched
back on."""

import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
UNITS = [REPO / ".config/systemd/user" / name
         for name in ("zita-j2n.service", "zita-n2j.service")]
REALTIME_KEYS = ("CPUSchedulingPolicy", "CPUSchedulingPriority", "LimitRTPRIO")


class ZitaLeavesRealtimeToJack(unittest.TestCase):
    def test_no_unit_raises_the_whole_process(self):
        for unit in UNITS:
            for line in unit.read_text().splitlines():
                for key in REALTIME_KEYS:
                    with self.subTest(unit=unit.name, key=key):
                        self.assertNotIn(key, line)

    def test_the_cores_stay_where_they_were(self):
        for unit in UNITS:
            with self.subTest(unit=unit.name):
                self.assertIn("CPUAffinity=1 2 3", unit.read_text().splitlines())


if __name__ == "__main__":
    unittest.main()
