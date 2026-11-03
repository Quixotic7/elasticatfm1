#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The E5 proof: the demo through panel input only (host/demo_e5.txt) -> results/e5/*.png + e5_demo.wav; its saved
# project rebooted (the E5 chunk back: scenes, the pattern's name, page loop); an E2 save (host/fixtures/e2_flash.bin,
# written by the 0.1.0 build) booting with the E5 defaults. After the native build:
#   host/render_e5.sh [build dir] [out dir]   (default build, results/e5)
set -e
B=${1:-build}
OUT=${2:-results/e5}
H=$(dirname "$0")
python3 "$H/make_demo_folder.py" "$B/demo_folder" >/dev/null
mkdir -p "$OUT"
rm -f "$B/e5_flash.bin"
"$B/ec_run" --layout test --folder "$B/demo_folder" --ms 22000 --out "$OUT" --script "$H/demo_e5.txt" --wav "$OUT/e5_demo.wav" \
    --flash "$B/e5_flash.bin" --press GLO@21600+300 --press SAVE@21700 \
    --rms-from 1300 --min-rms 0.02 --expect-ui sa:3 --expect-ui sb:3 --expect-ui memo:1 \
    --expect-ui pname:SMOOTH_OMIO --expect-ui keys:loop --expect-ui play:1 --expect-ui saves:1

# power on again: the E5 chunk comes back (0.6.0: scene B captured with the depth too: sb 3; ARP = CROSSFADER)
"$B/ec_run" --layout test --folder "$B/demo_folder" --flash "$B/e5_flash.bin" --ms 3000 --png-scale 2 --press PLAY@1500 \
    --press ARP@1600 --png-at 2500:"$OUT/27_reload_scene.png" \
    --rms-from 1600 --min-rms 0.02 --expect-ui sa:3 --expect-ui sb:3 --expect-ui xf:0.817 --expect-ui ready:2 \
    --expect-ui pname:SMOOTH_OMIO

# an E2 (0.1.0) save: loads, plays as before, the E5 fields at their defaults (0.6.0: PLAY acts on its press: 1500)
cp "$H/fixtures/e2_flash.bin" "$B/e2_flash.bin"
"$B/ec_run" --layout test --folder "$B/demo_folder" --flash "$B/e2_flash.bin" --ms 3000 --png-scale 2 --press PLAY@1500 \
    --png-at 2500:"$OUT/28_e2_save.png" --rms-from 1600 --min-rms 0.03 --expect-ui ready:4 --expect-ui patterns:2 \
    --expect-ui sa:0 --expect-ui gbpm:0 --expect-ui pname:- --expect-ui fired:3,4,2,7,0,0
