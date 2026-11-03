#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The panel's screens (0.6.0 gestures) through panel input only (host/demo_ux.txt) -> results/ux/*.png (+ ux_demo.wav, untracked).
#   host/render_ux.sh [build dir] [out dir]   (default build, results/ux)
set -e
B=${1:-build}
OUT=${2:-results/ux}
python3 "$(dirname "$0")/make_demo_folder.py" "$B/demo_folder" >/dev/null
mkdir -p "$OUT"
"$B/ec_run" --layout test --folder "$B/demo_folder" --out "$OUT" --script "$(dirname "$0")/demo_ux.txt" --wav "$OUT/ux_demo.wav" \
    --rms-from 1300 --min-rms 0.02 --expect-ui file:b_bass_bpm120.wav --expect-ui solo:00 --expect-ui mutes:00 \
    --expect-ui redo:0 --expect-ui popup:0 --expect-ui ready:2
