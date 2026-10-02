"""StemDeck's truth keeper, held to the truth and to its own start
(spec truth-from-core, step 3; final review 2026-10-02).

C++ without JUCE in the test runner: what needs the sources is checked here.
"""

import json
import os
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SOURCE = REPO / "Source"
KEEPER = (SOURCE / "TruthKeeper.h").read_text()


def truth():
    named = os.environ.get("A3_OSC_TRUTH")
    path = Path(named) if named else (
        REPO.parent / "a3-core/platform-config/debian-x86_64/a3-core/usr/share/a3/a3-osc.json")
    if not path.exists():
        raise unittest.SkipTest(f"no truth at {path}: set A3_OSC_TRUTH")
    return json.loads(path.read_text())


class TheBootstrapIsTheTruths(unittest.TestCase):
    """The two literals the keeper knows before it has a truth (a3-core's guard
    allows them by name) are the truth's own -- else the keeper goes deaf with
    every suite green."""

    def test_port_and_word(self):
        data = truth()
        port = next(l["port"] for l in data["listeners"]
                    if l["program"] == "devices" and l["role"] == "announce")
        self.assertEqual(int(re.search(r"announcePort = (\d+);", KEEPER).group(1)), port)
        self.assertEqual(re.search(r'announceAddress = "([^"]+)";', KEEPER).group(1),
                         data["addresses"]["core.here"]["pattern"])


class TheTruthIsResolvedOnce(unittest.TestCase):
    """The keeper's own fingerprint must be of the truth StemDeck loaded: the
    path is chosen once and handed on -- a second liveTruthPath() could land
    on a cache Motion wrote in between (final review)."""

    def test_one_call_outside_its_definition(self):
        calls = 0
        for path in SOURCE.glob("*.cpp"):
            text = path.read_text()
            if path.name == "OscTruthFile.cpp":
                text = text.replace("std::string liveTruthPath()", "")
            calls += len(re.findall(r"liveTruthPath\s*\(\s*\)", text))
        self.assertEqual(calls, 1)

    def test_the_keeper_is_given_the_hash_of_that_truth(self):
        main = (SOURCE / "MainComponent.cpp").read_text()
        self.assertIn("remote.start (truthPath, truthHash)", main)
        self.assertIn("SHA256 (juce::File (truthPath))", main)



class RadlaPolls(unittest.TestCase):
    """Step 4: where ~/.config/a3/core names Core, the keeper polls it."""

    def test_a_named_core_is_polled_else_heard(self):
        text = (SOURCE / "MainComponent.cpp").read_text()
        self.assertIn("truthkeeper::corePath (", text)
        self.assertIn("startPolling (", text)
        self.assertIn("->start()", text)

    def test_a_poll_with_our_own_header_writes_nothing(self):
        text = (SOURCE / "TruthKeeperLink.cpp").read_text()
        take = text[text.index("void TruthKeeperLink::take"):]
        self.assertIn("needsFetch", take)
        self.assertLess(take.index("needsFetch"), take.index("readIntoMemoryBlock"))


class ThePoolGoesLast(unittest.TestCase):
    """Final review of step 3: members are destroyed in reverse order, so a
    job the pool still waits for must find `busy` and the rest alive."""

    def test_the_thread_pool_is_the_last_member(self):
        header = (SOURCE / "TruthKeeperLink.h").read_text()
        body = header[:header.rindex("};")]
        pool = body.index("juce::ThreadPool fetcher")
        rest = body[pool:]
        self.assertNotIn("busy", rest)
        self.assertNotIn("refusals", rest)

if __name__ == "__main__":
    unittest.main()
