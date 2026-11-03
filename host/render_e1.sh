#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The E1/E2 proof: the demo built through panel input only (host/demo_e1.txt) -> results/e1/*.png + e1_demo.wav, the
# saved project rebooted from its flash image (24_reload.png; HOME: TRIG, e1_reload.wav), and the same project with one sample
# gone (25_missing.png). After the native build:  host/render_e1.sh [build dir] [out dir]   (default build, results/e1)
set -e
B=${1:-build}
OUT=${2:-results/e1}
python3 "$(dirname "$0")/make_demo_folder.py" "$B/demo_folder" >/dev/null
mkdir -p "$OUT"
rm -f "$B/e1_flash.bin"
"$B/ec_run" --layout test --folder "$B/demo_folder" --out "$OUT" --script "$(dirname "$0")/demo_e1.txt" --wav "$OUT/e1_demo.wav" --flash "$B/e1_flash.bin" \
    --rms-from 18700 --min-rms 0.03 --expect-ui ready:4 --expect-ui missing:0 --expect-ui patterns:2 \
    --expect-ui saves:1
cp "$B/e1_flash.bin" "$B/e1_flash_saved.bin"

# power on again: the project comes back from the flash, its samples from /audio
"$B/ec_run" --layout test --folder "$B/demo_folder" --flash "$B/e1_flash.bin" --ms 9000 --png-scale 2 --press PLAY@1500 \
    --png-at 1400:"$OUT/24_reload.png" --press HOME@4000 --png-at 5000:"$OUT/24b_reload_trig.png" \
    --wav "$OUT/e1_reload.wav" --rms-from 1600 --min-rms 0.03 --expect-ui ready:4 --expect-ui patterns:2 \
    --expect-ui play:1

# the same project with the chords gone: T3 says what is missing, the rest plays on
mkdir -p "$B/demo_missing/audio"
for f in a_drums_bpm120.wav b_bass_bpm120.wav d_perc16_bpm120.wav; do cp "$B/demo_folder/audio/$f" "$B/demo_missing/audio/"; done
cp "$B/e1_flash_saved.bin" "$B/e1_flash_missing.bin"
"$B/ec_run" --layout test --folder "$B/demo_missing" --flash "$B/e1_flash_missing.bin" --ms 4000 --png-scale 2 --press GLO@700+200 --lower 2@750 \
    --press PLAY@1000 --png-at 2500:"$OUT/25_missing.png" --rms-from 1100 --min-rms 0.02 --expect-ui missing:1 \
    --expect-ui ready:3
