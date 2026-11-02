The layout tool's round trip (2026-11-02): a design made in the designer, compiled into the cart, back to 0.4.0.

1. The designer (tools/layout/test_designer.mjs) swapped FILT / FILTER (classic) KNOB 1 CUT and KNOB 2 RES and
   exported results/layout/swap-cut-res.json; kept as the named design design/layout/designs/swap-cut-res.json.
2. That file as design/layout/elasticat.json: cmake --build build ran gen_layout.py, which rewrote
   src/app/ec_layout_gen.c (ec_layout_gen.diff: the two items swapped, the name) and docs/FM1-CONTROLS.md's FILTER row
   (RES | CUT | TYPE | DPTH). ctest: 22 of 22 passed (ctest.txt; layout_walk found every param of the swapped layout
   on its knob by id; the other tests run on the fixed 0.4.0 layout).
3. The native cart (build/ec_run, shipped layout, EDIT at 1.5 s): ec_run_filt_swapped.png, RES on K1, CUT on K2.
4. The .rfm1 rebuilt in the FM1VST worktree (rfm1-elasticat, -DEC_SOURCE=<elasticatfm1>, build-rfm1-layout):
   rfm1_run --press EDIT@1500 --screen: rfm1_filt_swapped.png, the same; ctest -R rfm1_elasticat 2 of 2 passed.
5. design/layout/elasticat.json restored: the generated C and the docs came back byte-identical to the commit.
