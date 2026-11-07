# QA 0.6.52: LOOP ENGINE, slice machines, SLICE editor (2026-11-06)

30 scripted runs (s1-s30) on fc47945 through build/ec_run: no crash, assert or ec_run error; ctest -R loopstep
passes. Rerun: `./run.sh N MS [wav]` (copies fresh/ to sN/folder, runs sN.txt into sN/; EXTRA="..." for flags);
`leds.py N MS..` prints the key LEDs; `tools.py info N field..` / `tools.py strip N`; `onsets.py sN/out.wav FROM TO`.
s21 reloads s20's folder: `./run.sh 20 12000; FOLDER=s20/folder ./run.sh 21 8600`. Per-run folders and video frames
were moved to the session scratchpad (loop-aside); run.sh makes them again.

## Bugs

| # | Sev | What | Repro (script @ ms) | Code |
|---|---|---|---|---|
| 1 | high | SHIFT + F1 in the SLICE editor runs EVEN, not undo (SHIFT + F2 runs AUTO-CHOP, not redo): undoing a chop redistributes the table to COUNT even slices and adds an undo entry each press. Undo works only after leaving the editor. | s12 @11000-12600; s13 @9000 | ec_panel.c:539 sends F1 / F2 to ec_rzui_oct before the SHIFT undo branch (541); ec_razor_ui.c:354-358 |
| 2 | medium | The machine changes while playing through B3's hold menu (UX-V2 §12: only while stopped; LOOP SETUP's MACHINE row and the warp menu say STOP FIRST). Regression of 0.6.15 bug 12. | s17 @6500-7600 | ec_holdmenu.c:217 checks only EC_P_MODE |
| 3 | medium | Slice locks have no undo (pad held + knob; F1 + knob LOCK CLEARED); the next SHIFT + F1 undoes the machine (UNDO MACH) instead. | s27 (8500 undo:1; 9200 still 1; 10200 UNDO MACH); s26 | ec_e5.c:700-712; ec_app.c:988-996, 1034-1035 |
| 4 | medium | A loop-key entry into steps on two step pages makes two undo entries (LEN 32, steps 1 + 18, W3+W4: undo count 1 -> 3); one SHIFT + F1 restores only step 18. B11 + OCT- clear makes one per page too. 0.6.38 promises one per gesture. | s11 | ec_loopstep.c:232-234 |
| 5 | low | The dice give fractional values to whole-step params: PITCH +5.4 (detuned), NOTE likewise; P/T, CUT, ENV times, BIP / PAN, CHNC (step 1) come out fractional; only EC_PF_INT is rounded. | s16 @9600-10800 | ec_app.c:968 ignores d->step |
| 6 | low | Slice tracks' step-entry message names a loop key: bank 17-32, pad W3 (slice 19) says "LOOP 3 > 2 STEPS" (cut "ST.."). | s9 @11300; s5 @9400 | ec_loopstep.c:262-266 |
| 7 | low | On a SLICE / S.POLY track the SLICE editor shows and edits the razor table those machines don't play (COUNT 4 plays even quarters; the editor shows slice 1 as 0.000-0.062, 1/16 widths). | s15 (06 vs 09) | partly ec_razor_ui.c:261-289 |

## Design questions

- Range locks stick, loop-region locks don't: a step's R-START / R-END lock keeps playing on later trigless steps
  (s29); LOOP START / END locks end with their note (s4). ec_seq.c:137 is_region covers only the loop points.
- Latched STEP layer + B11 held (the loop keys) swaps the cells for the pages panel (LEN / RATE): loops set without
  START / END on screen, K1 / K2 edit length and rate (s5 04).
- The RANGE page draws the whole file but its 16 keys span the trimmed audio: with a trim they don't line up (s15).
- A deleted slice's span belongs to no slice (dim). Merge into a neighbour? (s12 06)
- The target step can sit off the shown page: after PRESETS turns to page 2 the loop keys still write step 7; the
  next ALGO detent jumps to the shown page's first step (s6 07-08).
- Still open from 0.6.15: lowering COUNT without EVEN keeps old edges (s12, s15); KEYBOARD's B11 bank shows 11-20 with
  16 slices (LEDs now dark past COUNT) (s14 05).

## Cosmetic

- A SLOT OFF track shows slot 1's file name above "SLOT OFF" (s16).
- KEYBOARD's slice view marks the slice as a dark block between orange lines; LOOP ENGINE and the SLICE editor use the
  0.6.18 cream block (s14).
- The header's right box: P1 / P2 on LOOP and TRIG, a bare 3 / 4 / 5 on MACHINE, WARP, RANGE (s6).
- "LOOP 3 > 2 STEPS" cut to "ST.."; "LOOP 9-12 > 2" has no STEPS.
- Undoing a step entry turns the step page back to S1 (s11 05).

## Documentation mismatches (docs/FM1-CONTROLS.md unless noted)

- l.1082 "the loop's keys dim, the playhead's key mid": the loop's keys are lit, the playhead dim only outside the
  loop (ec_panel_leds.c:190-200).
- l.977 / 1090 / 323 "TRIG ... B11 unused": B11 latches / holds the STEP layer in TRIG (s24).
- l.83 "ALGO picks the step page ... PAGE 2/4": PRESETS does; ALGO selects a step or nothing.
- l.57 (PRESETS = the track's pool slot) and l.115 (PRESETS walks the used slots): stale; PRESETS turns the step page.
- l.219 "B1 (the quick setting) | LOOP ENGINE | MACHINE": B1 is TRIG (hold: ADVANCE); MACHINE is B3.
- l.575 "B1-B4 open its pages": B1-B5 since 0.6.41.
- The SHIFT table: F1 / F2 undo / redo with no SLICE editor exception; the SLICE section doesn't mention them (bug 1).
- design/layout/descriptions.json:479 (LDIV) "SHIFT + key reaches 17-32": SHIFT + key selects a track or nothing;
  17-32 is the B11 + K3 / PRESETS zoom (s28, s19).
- Line 1 still says "(0.6.32)".

## Checked and fine

- Loop keys stopped: neighbour toggle (1-2, 1, 1-2), a release keeps the span, a press refits it (s2); one undo per
  LIVE gesture (s2).
- Playing LIVE: brackets hidden unless a key is held; LEDs follow the playing region (s3). Stopped: a loop key
  auditions and writes the region (s3).
- STEP layer: keys held together = one selection, a separate press a new one; headers right (s4, s5). Entry into
  selected steps writes region locks and trigs every press; they play; one undo (s4).
- B11 latch; latched + B11 held = loop keys; B11 + OCT- clears the selection (s5).
- ALGO selects steps on LOOP, MACHINE, WARP, RANGE, TRIG page, TRIG mode, KEYBOARD; full left LIVE; nothing on
  FILTER (s6). PRESETS turns the step page with the S1 / S2 box (s6).
- B11 + ALGO / K1 length; B11 + K3 zoom 32 A / 32 B / 16 with the right keys and LEDs (s19).
- RANGE keys set the range; the LOOP view shows only the range; a crop, not a repitch (onsets stay 500 ms apart at
  full and quarter range) (s7, s29). Trim -> range -> slices: COUNT 4 spans the range, pads hit the right quarters
  (s15).
- SLICE via B3 stopped: velocity pads, cream block, LEDs ~10 % for 1..COUNT, dark past it (s8). COUNT 32: the 17-32
  bank and bar, W1 = slice 17, pad entry writes slice 19, ALGO walks each step's slices (s9).
- SLICE editor: COUNT, EVEN, AUTO-CHOP (slice 1 at 0.000), split, delete, mute, B11 preview, undo from outside (s12,
  s13). RAZOR table drawn on LOOP ENGINE; save / reload keeps range, step slices, table (s20-s23).
- KEYBOARD on a slice track: B1-B10 pick, B11 banks, a target step gets slice + pitch and advances (s14).
- SLOT OFF by default; PRESETS leaves it, K2 sets it, SHIFT + K2 reaches OFF, the dice keep it (s16).
- Warp menu STOP FIRST while playing; B5, B2, B1 hold menus work (s17). TRIG page lanes P1-P4, slice lane 4, ALGO +
  K1 NOTE lock (s18).
- Pad held + knob = slice lock; COUNT with a pad held edits the track; F1 + K1 clears it (s26). A step key in TRIG
  mode locks only that step (s24, s25).
- Re-checks of 0.6.15: Auto-Chop from 0 stays fixed; the slice table's undo exists but only from outside the editor
  (bug 1); machine-while-playing is back via B3 (bug 2).
