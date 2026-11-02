#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The 0.2.0 proof: host/demo_v020.txt through panel input -> results/v020/*.png + v020_demo.wav (untracked), then a
# reboot from the saved flash: the same project comes back (patterns, scenes and the fader, the razor table, the
# buses) and plays.  host/render_v020.sh [build dir] [out dir]
set -e
B=${1:-build}
OUT=${2:-results/v020}
python3 "$(dirname "$0")/make_demo_folder.py" "$B/demo_folder" >/dev/null
mkdir -p "$OUT"
rm -f "$B/v020_flash.bin"
"$B/ec_run" --folder "$B/demo_folder" --out "$OUT" --script "$(dirname "$0")/demo_v020.txt" \
    --wav "$OUT/v020_demo.wav" --flash "$B/v020_flash.bin" --info-at 16900 \
    --rms-from 9600 --min-rms 0.02 --expect-ui ready:3 --expect-ui missing:0 --expect-ui dirty:0 \
    --expect-ui patterns:2 >"$B/v020_run1.txt"
cat "$B/v020_run1.txt"
# the state to find again: the scene counts, the fader, the patterns, the samples
want=$(sed -n 's/.*ui_info: //p' "$B/v020_run1.txt" | tail -1 | tr ' ' '\n' | grep -E '^(sa|sb|xf|patterns|ready|missing|pat|files):')
set --
for w in $want; do set -- "$@" --expect-ui "$w"; done
"$B/ec_run" --folder "$B/demo_folder" --flash "$B/v020_flash.bin" --ms 4000 --png-scale 2 \
    --press PLAY@1500 --png-at 1400:"$OUT/13_reboot.png" --upper 8@2000+60 --upper 8@2100+60 --upper 8@2200+60 \
    --png-at 2600:"$OUT/14_reboot_scene.png" --rms-from 1600 --min-rms 0.02 "$@"
