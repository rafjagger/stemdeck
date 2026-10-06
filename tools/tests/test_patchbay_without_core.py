"""On a machine without the Core, a patchbay wires StemDeck to zita.

a3nuc2 runs StemDeck on its own: zita-j2n carries ten channels to the Core,
whose patchbay puts them on REAPER in13..in22 -- the inputs the Core's local
StemDeck uses for deck1_L..aux_R, in that order. Nothing connected StemDeck
to zita-j2n there; connected by hand, the sound arrived (2026-10-06). The
patchbay makes it permanent, and its unit keeps a QjackCtl running with it.

QjackCtl connects a cable plug by plug, in order, and reads a socket's client
as a regular expression ("zita\\-j2n"); both are checked against the names
the programs really register.
"""

import configparser
import re
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
PATCHBAY = REPO / ".config/rncbc.org/stemdeck-without-core.xml"
UNIT = REPO / ".config/systemd/user/qjackctl-stemdeck.service"
UNITS = REPO / ".config/systemd/user"
INSTALLED_REPO = "/home/aaa/a3-system/stemdeck"
INSTALLED_PATCHBAY = f"{INSTALLED_REPO}/.config/rncbc.org/stemdeck-without-core.xml"

# StemDeck's buses toward the Core, in Buses.h's order: 1-4, then AUX.
TO_THE_CORE = ["deck1", "deck2", "deck3", "deck4", "aux"]
STEMDECK_TO_ZITA = [(f"{bus}_{side}", f"in_{n}") for n, (bus, side) in enumerate(
    ((bus, side) for bus in TO_THE_CORE for side in "LR"), start=1)]


def stemdeck_ports():
    return [port for port, _ in STEMDECK_TO_ZITA]


def connections():
    """Every (output client, port, input client, port) the patchbay makes."""
    root = ET.parse(PATCHBAY).getroot()
    outputs = {s.get("name"): s for s in root.find("output-sockets")}
    inputs = {s.get("name"): s for s in root.find("input-sockets")}
    made = []
    for cable in root.find("cables"):
        out, into = outputs[cable.get("output")], inputs[cable.get("input")]
        out_plugs = [p.text for p in out.findall("plug")]
        in_plugs = [p.text for p in into.findall("plug")]
        assert len(out_plugs) == len(in_plugs), cable.attrib
        made += [(out.get("client"), o, into.get("client"), i)
                 for o, i in zip(out_plugs, in_plugs)]
    return made


def real(client_pattern, name):
    return re.fullmatch(client_pattern, name) is not None


class ThePatchbayIsQjackCtls(unittest.TestCase):
    def test_it_is_a_qjackctl_patchbay(self):
        text = PATCHBAY.read_text()
        self.assertTrue(text.startswith("<!DOCTYPE patchbay>"), text[:40])
        root = ET.fromstring(text)
        self.assertEqual("patchbay", root.tag)
        self.assertEqual("stemdeck-without-core", root.get("name"))
        self.assertTrue(root.get("version"))
        for part in ("output-sockets", "input-sockets", "slots", "cables"):
            self.assertIsNotNone(root.find(part), part)

    def test_every_socket_is_audio_and_not_exclusive(self):
        root = ET.parse(PATCHBAY).getroot()
        for socket in root.iter("socket"):
            self.assertEqual("jack-audio", socket.get("type"), socket.attrib)
            self.assertEqual("off", socket.get("exclusive"), socket.attrib)

    def test_every_cable_joins_sockets_of_equal_size(self):
        connections()


class TheCables(unittest.TestCase):
    def test_exactly_stemdeck_to_zita_and_zita_back_to_the_recorder(self):
        """The two channels back are REAPER's rec bus (out23/24 on the rig,
        where its patchbay feeds them into StemDeck:rec_L/R): StemDeck
        records the rig's mix here too (maintainer, 2026-10-06)."""
        expected = [("StemDeck", o, "zita-j2n", i) for o, i in STEMDECK_TO_ZITA]
        expected += [("zita-n2j", "out_1", "StemDeck", "rec_L"),
                     ("zita-n2j", "out_2", "StemDeck", "rec_R")]
        made = connections()
        self.assertEqual(len(expected), len(made), made)
        for (o_client, o, i_client, i), (o_pat, o_got, i_pat, i_got) in zip(expected, made):
            self.assertTrue(real(o_pat, o_client), (o_pat, o_client))
            self.assertTrue(real(i_pat, i_client), (i_pat, i_client))
            self.assertEqual((o, i), (o_got, i_got))

    def test_the_order_is_the_cores(self):
        """deck1_L..aux_R on in_1..in_10: the Core's zita-n2j out_1..10 land
        on REAPER in13..in22, where its local StemDeck puts deck1_L..aux_R."""
        self.assertEqual(
            ["deck1_L", "deck1_R", "deck2_L", "deck2_R", "deck3_L", "deck3_R",
             "deck4_L", "deck4_R", "aux_L", "aux_R"],
            [o for c, o, *_ in connections() if real(c, "StemDeck")])
        self.assertEqual([f"in_{n}" for n in range(1, 11)],
                         [i for _, _, c, i in connections() if real(c, "zita-j2n")])


class TheNamesAreTheProgramsOwn(unittest.TestCase):
    def test_stemdeck_registers_these_ports(self):
        """Outputs.h: buses::portName(channel / 2) + _L/_R; Buses.h: deck1..4
        for buses 0..3, aux for bus 4; the client is "StemDeck"."""
        buses = (REPO / "Source/Buses.h").read_text()
        outputs = (REPO / "Source/Outputs.h").read_text()
        main = (REPO / "Source/MainComponent.cpp").read_text()
        self.assertIn("constexpr int aux = 4;", buses)
        self.assertIn('bus == aux ? "aux"', buses)
        self.assertIn('"deck" + std::to_string (bus + 1)', buses)
        self.assertIn('(channel % 2 == 0 ? "_L" : "_R")', outputs)
        self.assertIn('jack.open ("StemDeck"', main)

    def test_zita_carries_as_many_channels_as_are_cabled(self):
        """zita-j2n --chan 10 registers in_1..in_10; zita-n2j --chan 1-2
        registers out_1, out_2."""
        j2n = (UNITS / "zita-j2n.service").read_text()
        n2j = (UNITS / "zita-n2j.service").read_text()
        self.assertIn("j2n --chan 10 ", j2n)
        self.assertIn("n2j --chan 1-2 ", n2j)
        self.assertEqual(10, sum(1 for c in connections() if real(c[2], "zita-j2n")))
        self.assertEqual(2, sum(1 for c in connections() if real(c[0], "zita-n2j")))


def section(name):
    parser = configparser.ConfigParser(strict=False, interpolation=None)
    parser.optionxform = str
    parser.read_string(UNIT.read_text())
    return parser[name]


def exec_lines(*keys):
    return [line.split("=", 1)[1] for line in UNIT.read_text().splitlines()
            if line.split("=", 1)[0] in keys]


class ItsUnit(unittest.TestCase):
    def test_it_runs_qjackctl_with_this_patchbay_active(self):
        self.assertEqual([f"/usr/bin/qjackctl -a {INSTALLED_PATCHBAY}"],
                         exec_lines("ExecStart"))

    def test_it_never_starts_a_server_itself(self):
        """-s would start JACK from QjackCtl's settings; the machine's JACK is
        whatever already runs there."""
        for line in exec_lines("ExecStart"):
            self.assertNotIn(" -s", line)
            self.assertNotIn("--start", line)

    def test_it_waits_for_the_screen_and_for_jack(self):
        self.assertEqual([f"{INSTALLED_REPO}/tools/a3-wait-for-the-screen",
                          "/usr/bin/jack_wait -w"], exec_lines("ExecStartPre"))
        self.assertEqual("DISPLAY=:0", section("Service").get("Environment"))

    def test_it_stops_only_its_own_process(self):
        """No pkill: systemd stops the unit's own process; a pattern would
        hit any QjackCtl on the machine."""
        self.assertEqual([], exec_lines("ExecStop", "ExecStopPost"))
        self.assertNotIn("pkill", UNIT.read_text())

    def test_it_comes_back_and_starts_at_login(self):
        self.assertEqual("on-failure", section("Service").get("Restart"))
        self.assertIn("default.target", section("Install").get("WantedBy", "").split())

    def test_it_needs_no_unit_of_the_core(self):
        for key in ("BindsTo", "Requires", "Requisite"):
            self.assertNotIn("a3-", section("Unit").get(key, ""), key)

    def test_its_patchbay_is_the_one_in_this_repository(self):
        self.assertEqual(INSTALLED_PATCHBAY,
                         f"{INSTALLED_REPO}/{PATCHBAY.relative_to(REPO)}")
        self.assertTrue(PATCHBAY.is_file())


if __name__ == "__main__":
    unittest.main()
