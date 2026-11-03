#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The 0.6.0 mockups (tools/mockups/README.md): screens + pop-ups (build/ec_mock_ux2), the FM-1 panels (headless
# Chromium), then the checks (120x120 grid, 16 colours). Exit 1 if anything fails.
#   sh tools/mockups/make_mockups.sh [build dir] [out dir]
set -e
REPO=$(cd "$(dirname "$0")/../.." && pwd)
BUILD=${1:-$REPO/build}
OUT=${2:-$REPO/results/ux-v2-mockups}
mkdir -p "$OUT"
cmake --build "$BUILD" --target ec_mock_ux2 >/dev/null
# the norns sprites (Elasticat for norns, read-only beside this repo): the intro logo and the CAT
ART=${ELASTICAT_NORNS:-$REPO/../Elasticat}/images
python3 -I "$REPO/tools/mockups/sprite_to_c.py" --bin "$BUILD/intro.bin" "$ART/q7logoanim.png"
python3 -I "$REPO/tools/mockups/sprite_to_c.py" --bin "$BUILD/cat.bin" "$ART/dancingcat.png"
"$BUILD/ec_mock_ux2" "$OUT" --intro "$BUILD/intro.bin" --cat "$BUILD/cat.bin"
python3 -I "$REPO/tools/mockups/sprite_to_c.py" --gif "$OUT/Intro.gif" --fps 10 --palette "$OUT/palette.json" "$ART/q7logoanim.png"
python3 -I "$REPO/tools/mockups/sprite_to_c.py" --gif "$OUT/Cat.gif" --fps 10 --cat --palette "$OUT/palette.json" "$ART/dancingcat.png"
node "$REPO/tools/mockups/render_panels.mjs" --out="$OUT"
python3 "$REPO/tools/mockups/check_png.py" "$OUT"
