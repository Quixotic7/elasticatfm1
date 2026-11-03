#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The layout designer: Elasticat's UI + engine to WebAssembly (tools/layout/ec_layout_wasm.c over every unit of src/)
# with the demo project, and the page, into one folder to serve.
#   sh tools/layout/build_layout.sh [out dir]      (default build/layout-web; ~20 s)
#   python3 -m http.server 8768 --directory build/layout-web   ->  http://127.0.0.1:8768/
# Needs: the native build (build/, for ec_run: the demo project is built through panel input, host/demo_v020.txt),
# Python 3, Emscripten (emcc on PATH, /opt/homebrew/bin/emcc, or ~/emsdk).
set -e
R=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${1:-$R/build/layout-web}
B=$R/build
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)

EMCC=$(command -v emcc || true)
[ -z "$EMCC" ] && [ -x /opt/homebrew/bin/emcc ] && EMCC=/opt/homebrew/bin/emcc
if [ -z "$EMCC" ] && [ -f "$HOME/emsdk/emsdk_env.sh" ]; then
    . "$HOME/emsdk/emsdk_env.sh" >/dev/null 2>&1
    EMCC=$(command -v emcc || true)
fi
[ -z "$EMCC" ] && { echo "build_layout: no emcc (Emscripten)" >&2; exit 1; }

# the native side: the generated layout up to date, ec_run for the demo project
cmake -S "$R" -B "$B" >/dev/null
cmake --build "$B" -j8 --target ec_run >/dev/null
STAGE=$B/layout-stage
mkdir -p "$STAGE"
python3 "$R/host/make_demo_folder.py" "$STAGE/demo" >/dev/null
if [ ! -f "$STAGE/demo.flash" ] || [ "$R/host/demo_v020.txt" -nt "$STAGE/demo.flash" ]; then
    # the 0.2.0 demo project (T1 drums with DESTROY + a send, T2 NEIGH, T3 HARMONIZER + LFO, T4 R.POLY, scenes),
    # built on the tests' fixed layout (the script walks its pages)
    mkdir -p "$STAGE/shots"
    "$B/ec_run" --layout test --out "$STAGE/shots" --folder "$STAGE/demo" --flash "$STAGE/demo.flash" \
        --script "$R/host/demo_v020.txt" >/dev/null
fi

# the shipped design: as JSON (the designer starts from it and diffs against it) and packed (the test compares the
# designer's own packing with the generator's)
python3 "$R/tools/layout/gen_layout.py" "$R/design/layout/elasticat.json" --quiet --pack "$OUT/shipped.pack"
cp "$R/design/layout/elasticat.json" "$OUT/shipped.json"
mkdir -p "$OUT/designs"
( cd "$R/design/layout" && for f in designs/*.json; do [ -f "$f" ] && cp "$f" "$OUT/designs/"; done; true )
( cd "$OUT/designs" && printf '[' && first=1 && for f in *.json; do [ -f "$f" ] && [ "$f" != index.json ] || continue;
  [ $first = 1 ] || printf ','; printf '"%s"' "$f"; first=0; done; printf ']\n' ) > "$OUT/designs/index.json"

SRCS=$(ls "$R"/src/plat/*.c "$R"/src/io/*.c "$R"/src/ui/*.c "$R"/src/dsp/*.c "$R"/src/seq/*.c "$R"/src/app/*.c)
FUNCS=_lw_boot,_lw_run,_lw_ms,_lw_rgba,_lw_load,_lw_error,_lw_use_shipped,_lw_pack,_lw_context,_lw_bus_fx,_lw_target,_lw_goto,_lw_f3,_lw_turn,_lw_press,_lw_state,_lw_modes,_lw_value,_lw_catalog,_lw_button,_lw_key,_lw_master,_lw_leds,_lw_wiring,_lw_labels,_malloc,_free
"$EMCC" -O2 -std=gnu99 -Wall -Wextra -ffp-contract=off -DEC_FS_NATIVE -DEC_SS_ARENA_MIB=24 \
    -I"$R/src/plat" -I"$R/src/plat/abi" -I"$R/src/io" -I"$R/src/ui" -I"$R/src/dsp" -I"$R/src/seq" -I"$R/src/app" \
    -I"$R/tools/layout" $SRCS "$R/tools/layout/ec_layout_pack.c" "$R/tools/layout/ec_layout_wasm.c" \
    -sMODULARIZE=1 -sEXPORT_NAME=ElasticatLayout -sENVIRONMENT=web,node -sALLOW_MEMORY_GROWTH=1 \
    -sINITIAL_MEMORY=96MB -sSTACK_SIZE=1MB -sEXPORTED_FUNCTIONS=$FUNCS \
    -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,stringToNewUTF8,HEAPU8 \
    --embed-file "$STAGE/demo@/demo" --embed-file "$STAGE/demo.flash@/demo.flash" \
    -o "$OUT/ec_layout.js"

for f in index.html designer.js layout_compile.js designer.css; do cp "$R/tools/layout/$f" "$OUT/$f"; done
cp "$R/design/layout/descriptions.json" "$OUT/descriptions.json"     # (the designer only: never in the cart)
mkdir -p "$OUT/fm1" && cp "$R"/tools/layout/fm1/* "$OUT/fm1/"         # the FM-1 panel, vendored from RemoteFM1
echo "build_layout: $OUT (serve it: python3 -m http.server 8768 --directory $OUT)"
