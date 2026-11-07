#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# E6's screens through panel input only (host/demo_e6.txt), with the chord loop as a stand-in live input (latency 256
# frames, off at 7.4 s) -> results/e6/*.png (+ e6_demo.wav, untracked). Checks: one take landed and saved as
# /data/rec/rec-0001.wav (T1 plays it), T2 on INPUT, the input off at the end.
#   host/render_e6.sh [build dir] [out dir]   (default build, results/e6)
set -e
B=${1:-build}
OUT=${2:-results/e6}
python3 "$(dirname "$0")/make_demo_folder.py" "$B/e6_demo_folder" >/dev/null
rm -f "$B/e6_demo_folder/data/elasticat/rec/"rec-*.wav
mkdir -p "$OUT"
"$B/ec_run" --layout test --folder "$B/e6_demo_folder" --out "$OUT" --script "$(dirname "$0")/demo_e6.txt" --wav "$OUT/e6_demo.wav" \
    --audio-in "$B/e6_demo_folder/audio/c_chords_bpm120.wav" --audio-in-latency 256 --audio-in-off-at 7400 --ms 9500 \
    --rms-from 1300 --min-rms 0.01 --expect-ui takes:1 --expect-ui rsaved:1 --expect-ui rpath:/data/rec/rec-0001.wav \
    --expect-ui ain:0 --expect-ui recpop:0 --expect-ui trk:2 --expect-ui rec:idle
test -s "$B/e6_demo_folder/data/elasticat/rec/rec-0001.wav"
