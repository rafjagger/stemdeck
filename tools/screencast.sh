#!/usr/bin/env bash
# Streamt Bildschirm und Ton eines entfernten Rechners hierher, nimmt ihn
# in recordings/ auf und zeigt ihn live an.
#
#   tools/screencast.sh [host]          Standard: a3nuc1_mango
#
# Drüben: der X-Bildschirm :0 (x11grab), H.264 per x264 auf der CPU, der Ton
# per JACK: ein Client "screencast" mit input_1 / input_2. Die Eingänge werden
# NICHT automatisch verbunden -- drüben selbst verbinden (qjackctl, jack_connect),
# solange der Stream läuft; unverbunden ist der Ton still.
# FPS (Standard 30) und CRF (Qualität, Standard 20; kleiner = besser) ebenso,
# NOVIEW=1 nimmt nur auf, ohne Live-Fenster; DURATION=60 hört nach 60 s auf.
#
# Der Rechner drüben spielt live: ffmpeg läuft dort mit niedrigster
# Priorität auf CPU 0. Bewusst nicht per GPU (VAAPI): auf dem amdgpu des
# a3nuc1 blieb ein Encoder 2026-09-29 in einem GPU-Reset hängen, samt allem,
# was danach den Bildschirm las. Die Übertragung geht
# durch ssh (keine Ports, verschlüsselt), hier wird nicht neu kodiert.
#
# Hier wird NIE Ton abgespielt: ein neuer Audio-Ausgang im laufenden Graphen
# beendet zita-j2n (2026-09-29). Der Ton geht nur in die Datei.
#
# PREVIEW=240x320: statt des ffplay-Fensters rohe BGRA-Bilder dieser Größe
# (15 fps) auf stdout -- so zeigt StemDeck den Screencast in seinem Fenster.
# Dazu auf Deskriptor 3 der Ton, nur zum Messen: 8 kHz, Stereo, s16le.
#
# NOREC=1: nur ansehen, keine Datei.
#
# Das Live-Fenster darf man schließen, die Aufnahme läuft weiter. Strg+C
# beendet sie; die .ts-Datei wird dann zu .mp4 umverpackt (verlustfrei).
set -euo pipefail

cd "$(dirname "$(readlink -f "$0")")/.."

HOST="${1:-a3nuc1_mango}"
FPS="${FPS:-30}"
CRF="${CRF:-20}"
DURATION="${DURATION:-}"
LIMIT="${DURATION:+-t $DURATION}"

mkdir -p recordings
OUT="recordings/screencast $HOST $(date +%Y-%m-%d\ %H-%M-%S).ts"

# Läuft drüben. ffmpegs JACK-Client "screencast" kommt und geht mit ihm.
read -r -d '' REMOTE <<EOF || true
set -e
export DISPLAY=:0
SIZE=\$(xdpyinfo | awk '/dimensions:/ { print \$2 }')
exec nice -n 19 taskset -c 0 ffmpeg -hide_banner -loglevel warning -nostdin \\
    -thread_queue_size 1024 -f x11grab -framerate $FPS -video_size "\$SIZE" -draw_mouse 1 -i :0 \\
    -thread_queue_size 1024 -f jack -channels 2 -i screencast \\
    -c:v libx264 -preset veryfast -tune zerolatency -crf $CRF -pix_fmt yuv420p -g $((FPS * 2)) \\
    -c:a aac -b:a 256k \\
    $LIMIT -f mpegts -
EOF

if [[ -n "${NOREC:-}" ]]; then
    OUT=/dev/null   # nur ansehen: tee schreibt ins Leere, nichts wird umverpackt
    echo "Nur ansehen, keine Aufnahme  (Strg+C beendet)" >&2
else
    echo "Aufnahme: $OUT  (Strg+C beendet)" >&2
fi

# Strg+C geht an alle drei (ssh, tee, ffplay); danach wird hier umverpackt.
trap ':' INT

if [[ -n "${PREVIEW:-}" ]]; then
    W="${PREVIEW%x*}"; H="${PREVIEW#*x}"
    ssh -o BatchMode=yes "$HOST" "bash -c $(printf '%q' "$REMOTE")" 2> >(grep -v '^Jack:' >&2) \
        | tee -p "$OUT" \
        | ffmpeg -hide_banner -loglevel error -nostdin -fflags nobuffer -flags low_delay \
                 -probesize 4000000 -analyzeduration 2000000 -i - \
                 -map 0:v -vf "fps=15,scale=$W:$H:force_original_aspect_ratio=decrease,pad=$W:$H:(ow-iw)/2:(oh-ih)/2" \
                 -pix_fmt bgra -f rawvideo pipe:1 \
                 -map 0:a -ac 2 -ar 8000 -f s16le pipe:3 \
        || true
elif [[ -n "${NOVIEW:-}" ]]; then
    ssh -o BatchMode=yes "$HOST" "bash -c $(printf '%q' "$REMOTE")" 2> >(grep -v '^Jack:' >&2) > "$OUT" || true
else
    # tee -p: wird das Live-Fenster geschlossen, schreibt tee trotzdem weiter.
    ssh -o BatchMode=yes "$HOST" "bash -c $(printf '%q' "$REMOTE")" 2> >(grep -v '^Jack:' >&2) \
        | tee -p "$OUT" \
        | ffplay -hide_banner -loglevel error -an -fflags nobuffer -flags low_delay -framedrop \
                 -window_title "Screencast $HOST" - \
        || true
fi

if [[ "$OUT" != /dev/null && -s "$OUT" ]]; then
    if ffmpeg -hide_banner -loglevel error -i "$OUT" -c copy -movflags +faststart "${OUT%.ts}.mp4"; then
        rm -f "$OUT"
        echo "Gespeichert: ${OUT%.ts}.mp4" >&2
    else
        echo "Gespeichert: $OUT" >&2
    fi
fi
