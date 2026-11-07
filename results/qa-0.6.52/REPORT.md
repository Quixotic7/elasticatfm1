# QA pass, Elasticat 0.6.52 (2026-11-06)

**Outcome:** A1-A5 fixed in 0.6.53 (docs/UX-V2.md §49, test_proj). The rest is open.

main at fc47945. Five parallel sweeps, about 140 scripted harness runs plus sanitizer builds and loader fuzzing:

| Area | Folder | Runs | Bugs (high / med / low) |
|---|---|---|---|
| LOOP ENGINE, slices, SLICE editor | loop/ | 30 | 1 / 3 / 3 |
| FILE, PROJECTS, naming, SAMPLER, persistence | files/ | 24 | 3 / 3 / 4 |
| MOD matrix, FILTER / AMP / FX, p-locks | mod/ | 25 | 0 / 3 / 2 |
| Tracks, MIX, PATTERN, PAGES, XFADE, F3, KEYBOARD, panel | panel/ | 30 | 0 / 6 / 8 |
| Code review, ASan / UBSan, loader fuzz, cart build | code/ | 51 + 2 + probes | 0 / 2 / 5 |

Each folder has a FINDINGS.md with every item, its repro script and time, and the code line. This report merges
them, drops duplicates and orders by what matters most.

**Health:** no crash, assert or harness error in any run. ASan + UBSan: ctest 31/31 and every replayed script clean.
Fuzzing 40,000 mutated project files found no memory error; every loaded project round-trips with its params in
range. A 4-minute stress run with 6 tracks ran at 154x real time with no drift. The cart builds and its tests pass
(3/3).

**Biggest risk:** some of your work can be lost without warning (A1-A5). After that, the most common problem is
undo: about a dozen actions either never reach the undo stack or land as the wrong entry (B1-B10).

Items I checked in the code myself are marked (confirmed).

## A. Data loss: fix first

| # | What | Repro | Code |
|---|---|---|---|
| A1 | **A failed save marks the project SAVED.** save_start clears the dirty flag before writing, and neither failure path (a write error, or 0.6.36's NOT SAVED: FILE CHANGED) sets it back. After a failed autosave, PROJECTS shows SAVED, LOAD X? is not asked, and loading drops the edits. Autosave never retries, although UX-V2 §32 says it keeps refusing. (confirmed) | files/s07.sh (two tabs), s05.txt (read-only folder) | ec_projfile.c:351; 400-405, 498-506 |
| A2 | **NEW runs even when saving your edits fails.** new_after is not cleared on failure (open_after is), so the new project replaces the unsaved one. (confirmed) | files/s06.txt | ec_projfile.c:883, 957-959 |
| A3 | **A name starting with "." destroys another project.** SHIFT+W1 types "." and only spaces are trimmed. ".ZEBRA" is saved as .ZEBRA.ecp, which is hidden from the list and is also ZEBRA's safety-copy path, so ZEBRA's next save overwrites and deletes it. Found by two agents. (confirmed) | files/s08.txt; code/probes/probe_dotname.c | ec_e5.c:313; ec_projfile.c:242 |
| A4 | **A damaged project file that still passes its checksum loads as an empty project** and says LOADED. The safety copy is never tried, and the next autosave writes the empty project over the file. The code comment says this "cannot happen". (confirmed) | code/probes/probe_badparse.c | ec_projfile.c:671-672, 750 |
| A5 | **The recorder's setup on a track with SLOT OFF records over slot 1.** OFF is clamped to slot 1, so T1's sample is replaced with no undo. This is 0.6.15 bug 5 again, now that tracks default to OFF. | files/s13a.txt | ec_rec.c:444; ec_app.c:61-64 |

## B. Undo gaps (SHIFT+F1 undoes the wrong thing, or nothing)

| # | What | Repro |
|---|---|---|
| B1 | **SLICE editor: SHIFT+F1 runs EVEN and SHIFT+F2 runs AUTO-CHOP, not undo / redo.** Trying to undo a chop wipes the table. The editor's own key map takes the keys before the global undo does, and its code comment says that's deliberate, so this is a clash to settle. (confirmed) | loop/s12, s13; ec_panel.c:539 |
| B2 | **F2 + knob (clear a param's mods, no source key held) adds no undo entry.** The next undo reverts an older edit and the routes stay gone. (confirmed) | mod/s06; ec_app.c:1186 |
| B3 | **A route made with a source key:** its first detent (SRC / DST) is not recorded. Undo leaves a dead "LFO1 CUTOFF 0.0" slot. | mod/s05; ec_modkeys.c:229-241 |
| B4 | **Slice locks** (pad held + knob, and F1 + knob clearing them) have no undo; the next undo reverts the machine. | loop/s27 |
| B5 | **A loop-key entry into selected steps on two step pages makes two undo entries.** One undo leaves step 1 changed. B11 + OCT- clear is the same. | loop/s11 |
| B6 | **Pattern cut / paste have no undo.** Paste overwrites a used pattern silently; cutting the playing pattern silences it with no way back. | panel/s04 |
| B7 | **Scene clear / capture / scene knob edits have no undo, and clear has no confirm.** | panel/s06 |
| B8 | **Page-loop changes have no undo** (hold, solo, span, F3 page). Undoing a span that grew the track leaves a wrong mask. | panel/s05 |
| B9 | **Live REC makes one undo entry per recorded step.** One bar pushes the rest of your undo history off the 16-deep stack. | panel/s11 |
| B10 | **Smaller gaps:** MIX hold-mute and ALL TRACKS ON; freeing a pool slot (its trim, BPM and gain are lost). | panel/s02; files/s23 |

## C. Wrong behaviour

| # | What | Repro |
|---|---|---|
| C1 | **The machine changes while playing through B3's hold menu.** LOOP SETUP and the warp menu say STOP FIRST. Regression of 0.6.15 bug 12. (confirmed) | loop/s17; ec_holdmenu.c:217 |
| C2 | **MIX: PRESETS walks T1..T6 → S1, then can't move.** (confirmed) | panel/s01; ec_panel.c:1016 |
| C3 | **MIX with S1 / S2 (or MST on P1 / P2) shows T1's cells, and the knobs do nothing.** | panel/s03b |
| C4 | **The edited step page isn't clamped when the length shrinks** (PAGES K1, B11+ALGO, undo, recall, or a shorter track). A white key then writes an invisible trig past the end, which plays once the length grows. | panel/s05b, s14 |
| C5 | **KEYBOARD: after the target advances, ALGO counts from the old selection**, so entry goes backwards and overwrites notes. | panel/s10b |
| C6 | **PAGES: a span whose second key is B9-B11 is ignored, and the first key's hold mutes its page.** | panel/s05 |
| C7 | **Dice:** whole-step params get fractions (PITCH +5.4 plays detuned; also CUT, ENV times, PAN, CHNC). INSERT 2's P1..P8 roll 0..128 instead of the mapped effect's range (RINGS MODE 70). (confirmed) | loop/s16; code/probe_dice_ins2; ec_app.c:963-971 |
| C8 | **Dice on MOD / MATRIX do nothing**; SEL walks the rows instead. The docs list MOD among the dice pages. | mod/s12 |
| C9 | **Trying a source key out leaves its source's level changed.** A hold that makes and then frees a route still moves LFO 1 DEPTH from 0 to +64, with no undo. | mod/s24 |
| C10 | **DELETE of the open project is re-saved at once by autosave** (the screen says SAVED); the docs say it stays open as EDITED. (confirmed) | files/s04; ec_projfile.c:821 |
| C11 | **A project's first save stores an empty internal NAME** (SETUP NAME is blank after a reload). SAVE AS +1, OVERWRITE and RENAME of another project keep the old internal name, so renaming GOW 2 from SETUP turns it into GOW 3. | files/s03, s09, s24 |
| C12 | **After FREE, the next LOAD puts the new file on the track still pointing at the freed slot**, which breaks "only the user sets a track's slot". | files/s21 |
| C13 | **A source key held while changing track:** its release cleans up on the new track. | mod/s19 |
| C14 | **REC arms live recording in PROJECTS and the SAMPLER.** | panel/s11 |
| C15 | **KEYBOARD: a velocity-100 note over a velocity-40 note keeps the old velocity lock.** | panel/p03 |
| C16 | **On SLICE / S.POLY tracks the SLICE editor edits the razor table, which those machines don't play.** | loop/s15 |
| C17 | **Labels:** B11-held ALGO / PRESETS say STEP / PAGE but set LENGTH / ZOOM. MIX, XFADE and PATTERN show ALGO "STEP PG", and XFADE / PATTERN's PRESETS change the track's sample. A slice track's step-entry message names a loop key. A damaged file says NOT FOUND. | panel/p01, p02; loop/s9; files/s18 |

## D. Design questions for you

1. **Output clipping:** there is a hard clamp and no limiter. 6 default tracks with sends clipped 0.7 % of samples. Add a soft limiter on the master?
2. **No autosave while playing:** a long jam lives only in RAM until a stop or SHIFT+SAVE. Save on the bar while playing?
3. **FILE CHANGED** is only an expiring header alert. Show a lasting cue on the PROJECTS card / header?
4. **SLICE editor:** keep SHIFT+F1 / F2 as EVEN / AUTO-CHOP, or make them undo / redo like everywhere else (B1)?
5. **Range locks stick on later trigless steps, while loop-region locks end with their note.** Make them the same?
6. **Lowering COUNT without EVEN keeps the old edges** (open since 0.6.15; you asked earlier about 16 → 10 → 16).
7. **The LFO / MOD ENV / VEL meter still moves while stopped**, unlike the 0.6.51 pictures.
8. **The dice on an LFO page roll its level and MODE.** Keep the level, as VOL is kept?
9. **Depth locks on steps don't show on the target cell**, only on MATRIX's DEPTH.
10. **Fast spins lose detents** on lists, MIX PRESETS and SHIFT+PRESETS (one step per tick).
11. **The RANGE page draws the whole file, but its keys span the trimmed audio.**
12. **The target step can sit off the shown page after PRESETS turns it.**
13. **POOL doesn't mark missing samples.** There's no "revert to file". NEW asks with no line under the card about the project you leave.

## E. Cosmetic

- The SHIFT + knob takeover covers the screen even when the cells are visible, and stays about 1 s after SHIFT.
- Header text is cut off: "NOT ON THIS P..", "LOOP 3 > 2 ST..", "F1 COPY/CLR L..". Knob labels are cut at 10 characters ("APP SETTIN").
- MATRIX: long target names run into DEPTH ("WARP RATE0.0"); a bare RATE next to WARP RATE is ambiguous.
- LFO DEPTH at 0 draws a two-way bar (the LFOs are one-way now).
- The keybed offers "~", which the font can't draw; its white-key letters are packed tight; its header doesn't name the project.
- A SLOT OFF track shows slot 1's file name; KEYBOARD's slice block differs from LOOP / SLICE.
- The header's right box reads P1 / 3 / P01 / 1 inconsistently.
- Still open from 0.6.15: CROSSFADER 127 vs 128; F3 "121.053%"; a cut pattern still drawn as used; CAPTURE's header says PLAY TO START.
- The NEW pop-up says LOADING; the cat plays for failed loads. PAGES draws 16 squares on a short last page. The SHIFT LEDs light LFO (no settings) and B4 RECALL with nothing memorized.

## F. Documentation

**FM1-CONTROLS.md** is the main gap. It still says 0.6.32 in places and contradicts the panel in about twenty spots:

- ALGO "picks the step page" (l.83, l.1376);
- PRESETS = the pool slot (l.57, l.115);
- the FILE cycle and its hold (l.36, l.341, the panel table);
- a load going to LOOP ENGINE (l.44);
- files named PROJ-001;
- POOL's F1 / B11;
- TRIG's "B11 unused";
- B1 / MACHINE (l.219);
- B1-B4 (l.575);
- the LED wording (l.1082);
- APP SETTINGS on SHIFT+HOME;
- hold SEQ = F3;
- the PAGES knobs;
- XFADE W12 = 127;
- live REC "every mode but FILE";
- the missing step-held + F1 and SLICE editor SHIFT keys.

**Other documents:**

- docs/MOD-MATRIX.md (0.5.0 spec) and docs/PROJECTS-DESIGN.md (the unbuilt key map) need a "superseded" note.
- design/layout/descriptions.json:479 (LDIV "SHIFT + key reaches 17-32") is wrong.
- The guide stops at 0.6.48; it has B11 as a step layer on FILTER / AMP / FX / MOD and labels B2 SLICE.

The per-area FINDINGS.md files have the line numbers.

## Re-checked from QA 0.6.15

**Still fixed:**
- Auto-Chop from 0;
- the EDITOR's undo merge;
- holding a mode button with no key changes nothing;
- MODE LOCK on the EDITOR while playing;
- "LAST PAGE";
- F2 with a source key held is undoable.

**Back again:**
- bug 12, the machine changing while playing (now through B3, C1);
- bug 5, the recorder writing over slot 1 (now through SLOT OFF, A5).

**Partly fixed:**
- bug 1: slice-table undo works, but only from outside the editor (B1);
- bug 6: F2 alone + a knob still has no undo (B2).

## Suggested order

1. **A1-A5:** small, contained fixes in ec_projfile.c, ec_e5.c and ec_rec.c, each with a test.
2. **C1, C2, C4, C5, C7:** short fixes with clear repros.
3. **Undo, B1-B10:** one pass through the edit paths that skip `ec_e5_edit_params` / `ec_undo_steps`; settle D4 first.
4. **Docs:** an FM1-CONTROLS.md rewrite against 0.6.52, plus a guide update.

## Housekeeping

- `results/qa-0.6.52/code/` holds about 1.1 GB of folder copies (runs/sweep/*/folder).
- `build-qa-asan/` (about 1.2 GB, gitignored) sits at the repo root.
- Both can be moved aside once you've looked.
