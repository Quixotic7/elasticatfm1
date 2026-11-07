#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The E4 proof: FX on every slot, the sends, the master insert and MOD through panel input only (host/demo_e4.txt)
# -> results/e4/*.png + e4_demo.wav (untracked). After the native build:  host/render_e4.sh [build dir] [out dir]
set -e
B=${1:-build}
OUT=${2:-results/e4}
python3 "$(dirname "$0")/make_demo_folder.py" "$B/e4_folder" >/dev/null
mkdir -p "$OUT"
"$B/ec_run" --layout test --folder "$B/e4_folder" --out "$OUT" --script "$(dirname "$0")/demo_e4.txt" --wav "$OUT/e4_demo.wav" \
    --rms-from 4500 --min-rms 0.01 --expect-ui ready:4 --expect-ui missing:0
