#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The E3 proof: host/demo_e3.txt through panel input -> results/e3/*.png + e3_demo.wav (untracked), then a reboot
# from the saved flash (the razor table comes back from its RZR1 chunk). host/render_e3.sh [build dir] [out dir]
set -e
B=${1:-build}
OUT=${2:-results/e3}
python3 "$(dirname "$0")/make_demo_folder.py" "$B/demo_folder" >/dev/null
mkdir -p "$OUT"
rm -f "$B/e3_flash.bin"
"$B/ec_run" --folder "$B/demo_folder" --out "$OUT" --script "$(dirname "$0")/demo_e3.txt" --wav "$OUT/e3_demo.wav" \
    --flash "$B/e3_flash.bin" --rms-from 5000 --min-rms 0.02 --expect-ui ready:2 --expect-ui dirty:0
# power on again: T1's auto-chopped razor table is back (SRC, then FILE / RAZOR)
"$B/ec_run" --folder "$B/demo_folder" --flash "$B/e3_flash.bin" --ms 3000 --png-scale 2 \
    --png-at 1400:"$OUT/e3_reload_src.png" --press SAVE@1600 --press SAVE@1750+60 \
    --png-at 2200:"$OUT/e3_reload_razor.png" --expect-ui ready:2
