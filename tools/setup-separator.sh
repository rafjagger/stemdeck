#!/usr/bin/env bash
# Installs the stem separator StemDeck runs in the background: a Python venv
# with Demucs and the CPU build of PyTorch, plus the htdemucs model.
#
#   tools/setup-separator.sh [venv]      default: ~/.local/share/StemDeck/separator
#
# Needs python3 with venv (apt install python3-venv) and, for decoding,
# ffmpeg (apt install ffmpeg) -- both installed by you, not by this script.
# About 1 GB on disk. Torch comes from the PyTorch CPU index on purpose: the
# default PyPI wheel pulls ~3 GB of CUDA libraries nobody here uses.
set -euo pipefail

VENV="${1:-$HOME/.local/share/StemDeck/separator}"
TORCH_VERSION="2.14.0"
DEMUCS_VERSION="4.1.0"

command -v python3 >/dev/null || { echo "python3 is missing" >&2; exit 1; }
command -v ffmpeg >/dev/null || echo "note: ffmpeg is missing -- run: sudo apt install ffmpeg" >&2

echo "venv:   $VENV"
python3 -m venv "$VENV"
"$VENV/bin/pip" install --upgrade pip
"$VENV/bin/pip" install "torch==$TORCH_VERSION" --index-url https://download.pytorch.org/whl/cpu
"$VENV/bin/pip" install "demucs==$DEMUCS_VERSION"
# numpy: demucs imports it, but neither it nor the CPU torch wheel declares it.
"$VENV/bin/pip" install numpy

echo "model:  htdemucs (downloaded once, ~80 MB)"
"$VENV/bin/python" -c "from demucs.pretrained import get_model; get_model('htdemucs')"

echo
echo "Done. StemDeck finds the separator at $VENV/bin/demucs."
[ "$VENV" = "$HOME/.local/share/StemDeck/separator" ] || \
	echo "Not the default place: add <VALUE name=\"separatorVenv\" val=\"$VENV\"/> to ~/.config/StemDeck/StemDeck.settings."
