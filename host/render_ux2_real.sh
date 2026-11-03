#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The firmware's real 0.6.0 screens, one per mockup and with the mockups' file names (host/mock_ux2.c MODES[]), reached
# through panel input only (host/ux2_real.txt) on the shipped layout -> results/ux-v2-real/*.png (untracked), then the
# owner's grid / palette rules on every PNG (tools/mockups/check_png.py). The folder: the demo folder
# (host/make_demo_folder.py) plus /audio/breaks (its loops under break names) and /audio/breaks/jungle, for FILE's
# folders, and /audio/kit (32 copies of the percussion bar, loaded one by one: a full-ish pool); the chords loop as
# SAMPLER's live input.
#   host/render_ux2_real.sh [build dir] [out dir]   (default build, results/ux-v2-real)
set -e
B=${1:-build}
OUT=${2:-results/ux-v2-real}
H=$(dirname "$0")
F="$B/ux2_real_folder"
python3 "$H/make_demo_folder.py" "$F" >/dev/null
mkdir -p "$F/audio/breaks/jungle" "$F/audio/kit" "$OUT"
for i in 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32; do
    cp "$F/audio/d_perc16_bpm120.wav" "$F/audio/kit/hit_$i.wav"
done
cp "$F/audio/a_drums_bpm120.wav" "$F/audio/breaks/amen_170.wav"
cp "$F/audio/d_perc16_bpm120.wav" "$F/audio/breaks/think_96.wav"
cp "$F/audio/a_drums_bpm120.wav" "$F/audio/breaks/funky_drummer.wav"
cp "$F/audio/c_chords_bpm120.wav" "$F/audio/breaks/apache.wav"
cp "$F/audio/b_bass_bpm120.wav" "$F/audio/breaks/jungle/reese.wav"
rm -f "$F/data/elasticat/rec/"rec-*.wav
"$B/ec_run" --folder "$F" --out "$OUT" --script "$H/ux2_real.txt" --ms 130000 \
    --audio-in "$F/audio/c_chords_bpm120.wav" --expect-ui pool:36 --expect-ui takes:1 --expect-ui fdir:/audio/breaks
# every mockup has its real screen (host/mock_ux2.c MODES[]; 0.6.1: no mode pop-ups, so no *_popup shots)
for n in Main Shift Fx Filter Amp Mod Mix Pages Loop Trig Keyboard FileBrowser FileEditor Sampler Crossfader Patterns; do
    test -s "$OUT/${n}_screen.png" || { echo "missing $n"; exit 1; }
done
for n in Filter_env Mod_env Record AppSettings Intro Cat; do
    test -s "$OUT/${n}_screen.png" || { echo "missing $n"; exit 1; }
done
# 0.6.1 gfx: the three step-lock cell states, a held source key's depth bars
for n in Lock_held Lock_outline Lock_none Mod_held; do
    test -s "$OUT/$n.png" || { echo "missing $n"; exit 1; }
done
# the theme tables for the check: the mockups' palette.json (host/mock_palette.h, the same tables as src/ui/ec_palette.c)
mkdir -p "$B/ux2_real_pal"
"$B/ec_mock_ux2" "$B/ux2_real_pal" >/dev/null
cp "$B/ux2_real_pal/palette.json" "$OUT/palette.json"
python3 "$H/../tools/mockups/check_png.py" "$OUT"
