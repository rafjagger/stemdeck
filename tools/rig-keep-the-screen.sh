#!/usr/bin/env bash
# Keeps the rig's screen where it was while StemDeck (re)starts.
#
#   rig-keep-the-screen.sh remember   before StemDeck starts (ExecStartPre)
#   rig-keep-the-screen.sh restore    after it started (ExecStartPost)
#
# StemDeck lives on i3 workspace 4, but its start takes the screen: JUCE builds
# the window three times as it comes up and the first one takes the focus, so
# i3 showed workspace 4 instead of A3 Motion on every (re)start (2026-09-30;
# no i3 rule stops it). This puts back whatever workspace was showing.
set -u
STATE="${XDG_RUNTIME_DIR:-/tmp}/stemdeck-visible-workspace"

visible_workspace() {
    i3-msg -t get_workspaces | python3 -c \
        'import json,sys; print(next((w["num"] for w in json.load(sys.stdin) if w["focused"]), 1))'
}

case "${1:-}" in
    remember)
        visible_workspace > "$STATE" 2>/dev/null || echo 1 > "$STATE"
        ;;
    restore)
        # Until its window is full screen -- the last thing it asks for, and
        # the last thing that takes the screen -- then put the screen back.
        for _ in $(seq 1 60); do
            i3-msg -t get_tree | python3 -c '
import json, sys
def found(n):
    props = n.get("window_properties") or {}
    if props.get("class") == "StemDeck" and n.get("fullscreen_mode"):
        return True
    return any(found(c) for c in n.get("nodes", []) + n.get("floating_nodes", []))
sys.exit(0 if found(json.load(sys.stdin)) else 1)' && break
            sleep 0.5
        done
        sleep 1
        i3-msg -q "workspace number $(cat "$STATE" 2>/dev/null || echo 1)"
        ;;
    *)
        echo "usage: $0 remember|restore" >&2
        exit 2
        ;;
esac
exit 0
