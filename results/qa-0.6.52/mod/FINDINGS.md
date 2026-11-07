# QA 0.6.52: MOD matrix, sound-shaping pages, p-locks

25 scripts (sNN.txt, out/ with PNGs, logs, WAVs; run.sh, view.py, rmsenv.py), each on a fresh folder copy. No
crashes, asserts or ec_run errors.

## Bugs

| # | Sev | What | Repro (script @ ms) | Code |
|---|---|---|---|---|
| 1 | Med | F2 + knob with no source key held ("MOD CLEARED", every route on the param) makes no undo entry; the next SHIFT+F1 undoes the earlier depth edit (UNDO DPTH) and the route stays gone. 0.6.15 bug 6 is fixed only for F2 with a source key held (s07). | s06 @7800, undo @8600, matrix @10200 | src/app/ec_app.c:1186-1207 clear_mod() has no ec_e5_edit_params (cf. ec_modkeys.c:218) |
| 2 | Med | A route made with a source key: the first detent writes SRC/DST with no undo entry; only depth detents are recorded. Undo leaves "LFO1 CUTOFF 0.0" in MATRIX row 1: a dead slot, and the cell keeps its LFO1 mark. | s05 @8200-10200 | src/app/ec_modkeys.c:229-237 vs :241 |
| 3 | Med-low | Trying a source key changes the source's level for good: B6 held, K1 one detent, let go: the route is freed but LFO 1 DEPTH went 0 -> +64 and stays; no undo entry ("NOTHING TO UNDO"). Same for LFO 2, MOD ENV, VEL. | s24 @6600 / 8500 / 9600 | ec_modkeys.c:126-133 sets 128; release() :155-161 never restores it |
| 4 | Low | Changing track (SHIFT+W2) while a source key is held: the release runs on the new track; T1 keeps the depth-0 route, and on T2 a depth-0 slot in the same position could be cleared. | s19 @6900, T1 matrix @9200 | ec_modkeys.c:15 tv() is the current track, used in release(); MK->created keeps no track |
| 5 | Low | F1 + SEL on MOD/MATRIX: no dice; SEL walks the rows and turns onto LFO 1. Docs list MOD as a dice page. | s12 @9200-9950 | ec_app.c:931 MATRIX cells are EC_IT_MTX, so the page is not randomizable; ec_panel.c:1187 falls through |

## Design questions

- Depth locks on steps are invisible on the target cell: after B6 + W1 + K1, holding W1 shows nothing on CUTOFF;
  only MATRIX's DEPTH cell has the orange frame (s14).
- The LFO / MOD ENV / VEL meter keeps moving while stopped (ec_scr_e4.c:255-267), though 0.6.51 says a stopped
  screen shows the values set.
- The macro keys can't reach LFO DEPTH or SPEED (NOT MODDABLE, s20). Intended?
- The dice on an LFO page roll its level and MODE (s12: DEPTH +9, MODE TRIG), weakening every route from it. Keep the
  level, as VOL is kept?
- PRESETS on a 16-step pattern gives no feedback on FILTER / AMP / FX / MOD (ec_panel.c:995).

## Cosmetic

- SHIFT + a knob brings the full-screen takeover although the cells are visible (plain snap s21; source key + SHIFT
  s04 @8000, s26 @7700); it stays ~1 s after SHIFT is let go and hides the base + amount picture.
  ec_app_cells_visible() returns 0 while SHIFT's screen is up (ec_app.c:1177).
- Header text cut off: "NOT ON THIS P.." (s11), "F1 COPY/CLR L.." (s12).
- MATRIX rows: long names run into the DEPTH column ("WARP RATE0.0", s15); a bare "RATE" next to "WARP RATE" is
  ambiguous.
- LFO DEPTH at 0 draws a half-filled two-way bar, though the LFOs are one-way since 0.6.51 (s09).
- The takeover's depth bar has no centre mark (s04).
- Undoing the send mode says "UNDO TAP" (s13).

## Documentation mismatches

- docs/FM1-CONTROLS.md:83 still says "ALGO picks the step page" (contradicts line 56 and 0.6.52).
- docs/FM1-CONTROLS.md:127 lists MOD as a dice page (bug 5).
- docs/MOD-MATRIX.md is the 0.5.0 spec with no "superseded" note: MOD KEYS mode (hold MOD 400 ms), OCT+ clearing a
  route, NO MOD / FULL in the takeover (now NOT MODDABLE / MATRIX FULL in the header), bipolar LFOs.

## Re-checked from qa-0.6.15

Bug 6 fixed with a source key held (s07: undo and redo of MOD CLEAR work); still open for F2 alone + a knob (bug 1).

## Checked and fine

- B6 + K1 makes a route; depth 0.1 steps, one decimal; SHIFT snaps 25 / 50 and to 0; cell LFO1>CUTOFF with a
  centre bar; header LFO 1 on the blue bar.
- Sound follows modulation: LFO1>CUT +50 % on a low cutoff moves the spectral centroid from mean 200 Hz (std 110) to
  726 Hz (std 947) at the same RMS (s04 vs s04b); -50 % on a high cutoff only lowers it, 1458 -> 543 Hz (s22 vs
  s22b), so the one-way LFOs work as documented; LFO1>LEVEL 100 % raises RMS 0.116 -> 0.178, peak 0.999, no hard
  clipping (s17 vs s17b); --min-rms passes.
- Pictures: stopped frames identical (s18); playing they move; a held source key shows base + full amount, still; a
  held step freezes them at the step's value.
- Marks on FILTER, AMP ENV, FX SENDS; NOT MODDABLE and MATRIX FULL where expected; a hold left at 0 frees its route.
- MATRIX: K1 SRC, K2 DST, K3 DEPTH; SHIFT + DST jumps a category; SEL past row 8 onto LFO 1 and back to row 8; ALGO
  nothing; undo of these works.
- LFO route cursor: "> OFF" / NO ROUTE with none; SEL through the routes then the pages; ALGO the shown depth; no
  count.
- Source key taps: B6 on FILTER nothing; B8 on AMP opens MOD ENV; on MOD B6, B9, B10, B11 open their pages.
- P-locks: step + K1 / K2 locks; step + F1 clears only the last knob's lock, NOT ON THIS PAGE elsewhere; F1 + K1
  clears all of a param's locks; undo restores them.
- ALGO does nothing on FILTER, FILTER ENV, AMP, FX, MOD.
- Dice on FILTER: shake, throw, RANDOM 4; one undo restores all.
- FX SENDS: ALGO PRE / POST with the picture following; undo; B6 + K3 routes SEND 1.
- Source key + held step makes a depth lock; the slot survives the release; playing applies it.
- SHIFT snaps on CUTOFF: 20K -> 3.7K -> 650 -> 114 -> 20.
