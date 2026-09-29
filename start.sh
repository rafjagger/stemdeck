#!/usr/bin/env bash
# Baut StemDeck bei Bedarf und startet es.
#
# Audio: 10 JACK-Ports (deck1_L ... deck4_R, aux_L, aux_R). Läuft ein jackd
# (JACK2), wird dieser benutzt; sonst, wenn PipeWire da ist, dessen
# JACK-Schnittstelle über pw-jack. Ohne beides fällt StemDeck auf ALSA zurück.
#
# StemDeck übernimmt Samplerate und Puffergröße des Graphen, wie sie sind, und
# resampelt die Stems selbst. Es fordert absichtlich nichts an: eine Änderung
# im laufenden Graphen beendet z. B. zita-j2n. Die Rate also vorher festlegen,
# z. B. für PipeWire (bis zum Neustart):
#   pw-metadata -n settings 0 clock.force-rate 44100
set -euo pipefail

cd "$(dirname "$(readlink -f "$0")")"

BUILD_DIR=build
APP="$BUILD_DIR/StemDeck_artefacts/Release/StemDeck"

# JUCE installed under ~/local/juce (as on the Core NUC) is found without
# anyone having to set a path; a CMAKE_PREFIX_PATH set by hand still wins.
export CMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH:-$HOME/local/juce}"

# CMake's default generator: Ninja is not installed everywhere StemDeck runs
# (the Core NUC has none).
if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
fi

# Inkrementell: kompiliert nur, wenn sich etwas geändert hat
cmake --build "$BUILD_DIR" -j"$(nproc)"

if ! pgrep -x 'jackd|jackdbus' >/dev/null && command -v pw-jack >/dev/null && pgrep -x pipewire >/dev/null; then
    exec pw-jack "$APP" "$@"
fi

exec "$APP" "$@"
