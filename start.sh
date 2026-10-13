#!/usr/bin/env bash
# Baut StemDeck bei Bedarf und startet es.
#
# Audio: 12 JACK-Ports (deck1_L ... deck4_R, aux_L/R, dann cue_L/R). Ist PipeWire da,
# dient dessen JACK-Schnittstelle (über pw-jack, falls nicht ohnehin systemweit);
# ohne fällt StemDeck auf ALSA zurück.
#
# StemDeck übernimmt Samplerate und Puffergröße des Graphen, wie sie sind, und
# resampelt die Stems selbst. Es fordert absichtlich nichts an: eine Änderung
# im laufenden Graphen stört die anderen Clients. Die Rate also vorher festlegen
# (bis zum Neustart):
#   pw-metadata -n settings 0 clock.force-rate 44100
set -euo pipefail

cd "$(dirname "$(readlink -f "$0")")"

BUILD_DIR=build
APP="$BUILD_DIR/StemDeck_artefacts/Release/StemDeck"

# JUCE installed under ~/local/juce (as on the Core NUC) is found without
# anyone having to set a path; a CMAKE_PREFIX_PATH set by hand still wins.
export CMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH:-$HOME/local/juce}"

# CMake schreibt den absoluten Pfad des Checkouts in seinen Cache. Wurde der
# Ordner verschoben, passt der nicht mehr und jeder Build bricht ab: dann den
# Cache verwerfen und hier neu konfigurieren (die Objektdateien bleiben).
if [[ -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    configured="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$BUILD_DIR/CMakeCache.txt")"
    if [[ "$configured" != "$PWD" ]]; then
        echo "Checkout verschoben ($configured -> $PWD): konfiguriere neu." >&2
        rm -rf "$BUILD_DIR/CMakeCache.txt" "$BUILD_DIR/CMakeFiles"
    fi
fi

# CMake's default generator: Ninja is not installed everywhere StemDeck runs
# (the Core NUC has none).
if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
fi

# Inkrementell: kompiliert nur, wenn sich etwas geändert hat
cmake --build "$BUILD_DIR" -j"$(nproc)"

if command -v pw-jack >/dev/null && pgrep -x pipewire >/dev/null; then
    exec pw-jack "$APP" "$@"
fi

exec "$APP" "$@"
