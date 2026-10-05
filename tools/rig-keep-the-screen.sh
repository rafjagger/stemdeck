#!/usr/bin/env bash
# Keeps the rig's screen where it was while StemDeck (re)starts.
#
#   rig-keep-the-screen.sh remember   before StemDeck starts (ExecStartPre)
#   rig-keep-the-screen.sh restore    after it started (ExecStartPost)
#
# StemDeck lives on i3 workspace 2, but its start takes the screen: JUCE builds
# the window three times as it comes up and the first one takes the focus, so
# i3 showed StemDeck's workspace instead of A3 Motion on every (re)start (2026-09-30;
# no i3 rule stops it). This puts back whatever workspace was showing -- unless
# it holds no window: on a machine with StemDeck alone that is the empty
# workspace 1, and going back to it looked as if StemDeck had stopped
# (a3nuc2, 2026-10-05). Then StemDeck stays on the screen.
set -u
STATE="${XDG_RUNTIME_DIR:-/tmp}/stemdeck-visible-workspace"

# Whether workspace number $1 holds a window.
has_a_window() {
    i3-msg -t get_tree | python3 -c '
import json, sys
def windows(node):
    return bool(node.get("window")) or any(
        windows(n) for n in node.get("nodes", []) + node.get("floating_nodes", []))
def workspaces(node):
    if node.get("type") == "workspace":
        yield node
    for n in node.get("nodes", []):
        yield from workspaces(n)
num = int(sys.argv[1])
sys.exit(0 if any(windows(w) for w in workspaces(json.load(sys.stdin))
                  if w.get("num") == num) else 1)' "$1"
}

visible_workspace() {
    i3-msg -t get_workspaces | python3 -c \
        'import json,sys; print(next((w["num"] for w in json.load(sys.stdin) if w["focused"]), 1))'
}

case "${1:-}" in
    remember)
        visible_workspace > "$STATE" 2>/dev/null || echo 1 > "$STATE"
        ;;
    restore)
        # Until its main window is there -- the last of the three JUCE builds
        # as it comes up -- and a moment more, then put the screen back.
        for _ in $(seq 1 60); do
            xdotool search --name '^StemDeck$' >/dev/null 2>&1 && break
            sleep 0.5
        done
        sleep 2
        back="$(cat "$STATE" 2>/dev/null || echo 1)"
        has_a_window "$back" && i3-msg -q "workspace number $back"
        ;;
    *)
        echo "usage: $0 remember|restore" >&2
        exit 2
        ;;
esac
exit 0
