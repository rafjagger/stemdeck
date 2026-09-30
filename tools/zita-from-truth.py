#!/usr/bin/env python3
"""Start zita-j2n or zita-n2j with the address and port from the one truth.

  zita-from-truth.py j2n --chan 10 --24bit    -> zita-j2n ... <Core> <port>
  zita-from-truth.py n2j --chan 1-2           -> zita-n2j ... 0.0.0.0 <port>

Every address and port of the A3 system lives in a3-core's a3-osc.json
(decided 2026-09-30); on radla a copy sits where the package puts it on the
Core, /usr/share/a3/a3-osc.json (or the file $A3_OSC_TRUTH names). The j2n
unit sent to 192.168.43.129 -- Core's address before the rig moved to
192.168.8.x -- and UDP never said that nobody listened.

The channel counts stay in the units: they are StemDeck's, not the truth's.
"""

import json
import os
import sys
from pathlib import Path


def truth_path():
    return Path(os.environ.get("A3_OSC_TRUTH") or "/usr/share/a3/a3-osc.json")


def _listener(truth, program, role):
    for listener in truth["listeners"]:
        if listener["program"] == program and listener["role"] == role:
            return listener
    sys.exit(f"zita-from-truth: a3-osc.json has no listener {program}.{role}")


def zita_command(truth, which, args):
    """The zita command line for `which` ("j2n" or "n2j"), run on radla."""
    if which == "j2n":
        # Core's n2j listens on every interface of the Core machine: from
        # radla that is Core's own address.
        listener = _listener(truth, "zita-n2j", "audio")
        host = "core" if listener["host"] == "any" else listener["host"]
        return ["zita-j2n", *args, truth["hosts"][host], str(listener["port"])]
    if which == "n2j":
        # Every interface: radla's address is the truth's to say to the
        # others, not something to bind to here.
        listener = _listener(truth, "radla", "zita-n2j")
        return ["zita-n2j", *args, truth["hosts"]["any"], str(listener["port"])]
    sys.exit(f"zita-from-truth: j2n or n2j, not {which!r}")


def main(argv):
    if not argv:
        sys.exit(__doc__)
    path = truth_path()
    if not path.exists():
        sys.exit(f"zita-from-truth: no a3-osc.json at {path} -- copy the Core's "
                 "/usr/share/a3/a3-osc.json there")
    command = zita_command(json.loads(path.read_text()), argv[0], argv[1:])
    print(" ".join(command), flush=True)
    os.execvp(command[0], command)


if __name__ == "__main__":
    main(sys.argv[1:])
