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
"$BUILD/ec_mock_ux2" "$OUT"
node "$REPO/tools/mockups/render_panels.mjs" --out="$OUT"
python3 "$REPO/tools/mockups/check_png.py" "$OUT"
