# QA 0.6.52: FILE mode and persistence

24 scripted scenarios on fc47945; no crashes, asserts or harness errors. Each: sNN.txt, out_sNN/ (log.txt, PNGs,
strip.png), its folder f_sNN/ (copies of base/ = build/ux2_real_folder without data/). Helpers linking
build/libelasticat.a (no src/ change): gen.c (a rich random project file; its own load -> save is byte-identical) and
peek.c (a file's internal name, slots, tracks). s07.sh runs the two-tab conflict in real time.

## Bugs

| # | Sev | What | Repro | Code |
|---|---|---|---|---|
| 1 | high | A failed save marks the project SAVED: save_start clears A->dirty before writing; neither failure (write error, 0.6.36 NOT SAVED: FILE CHANGED) restores it. After a failed autosave PROJECTS shows SAVED, LOAD X? isn't asked, and a load silently drops the edits. Autosave never retries. | s07.sh (other tab writes ~1 s, PITCH +5 @60000, B11 LOAD @66000: edit gone); s05.txt (read-only projects dir) | ec_projfile.c:351; failure paths 400-405, 498-506 |
| 2 | high | NEW goes ahead when the save of the edits fails: new_after isn't cleared on failure (open_after is), so new_now() replaces the unsaved project. | s06.txt (edit @6100, B1 @7000, OCT+ @7400 -> DIGITAL BEACH, edits lost) | ec_projfile.c:883, 957-959 vs 404, 503 |
| 3 | high | A name starting with "." (SHIFT+W1 types ".", only spaces trimmed): ".ZEBRA" is written as .ZEBRA.ecp, hidden from the list and also ZEBRA's hidden safety copy, which ZEBRA's next save overwrites and deletes. | s08.txt @14500-21100 (GOW 2 gone from disk) | ec_e5.c:313, 237; list filter ec_projfile.c:242 |
| 4 | medium | DELETE of the open project is undone at once: dirty set without dirty_ms, autosave rewrites the file (saves 1 -> 2), the screen says SAVED. Docs: it stays open as EDITED. | s04.txt (B6 @7200, OCT+ @8000) | ec_projfile.c:821 + ec_app.c:1459 |
| 5 | medium | The recorder's setup on a SLOT OFF track records over slot 1 (OFF clamped to 1): T1's sample replaced, no undo. 0.6.15 bug 5 again now tracks default to OFF. | s13a.txt @12000-17500 (rslot:1 on trk:2) | ec_rec.c:444, ec_app.c:61-64 |
| 6 | medium | A project's first save stores an empty internal NAME (packed before the name is chosen): every NEW / fresh-boot file has name '' on disk; after a reload SETUP NAME is empty. | s03.txt + ./peek | ec_projfile.c:345 vs 477-482 |
| 7 | low | SAVE AS +1, OVERWRITE and RENAME of another project keep the old internal name: GOW 2.ecp says GOW, SHIFT+EDIT shows NAME GOW, and OK renames the file GOW 2 -> GOW 3. | s09.txt, s24.txt | ec_projects.c:132, 187; ec_projfile.c:786-800 |
| 8 | low | After a FREE, the next LOAD makes the track play the new file: T1 stays on freed slot 1, the LOAD fills slot 1, T1 plays b_bass (rule: only the user sets a track's slot). | s21.txt | behaviour |
| 9 | low | A damaged project file says NOT FOUND (the missing hidden copy's error); the cat dances over the failed load. | s18.txt @7600 | ec_projfile.c:704-713 |
| 10 | low | Freeing a pool slot (POOL F2 / SAMPLES B9) can't be undone; its trim, BPM, steps, gain are lost. | s23.txt @10500 | behaviour |

## Design questions

- No autosave while playing: a long jam's edits are only in RAM until a stop or SHIFT+SAVE.
- "X SAVED FIRST" can stay under the card while autosave saves beneath the waiting LOAD X?.
- NEW asks with no line under the card saying what yes does to the project you leave (LOAD and OVERWRITE have one).
- The cursor doesn't follow NEW or a load to the open project (SAVE +1 does).
- No "revert to file": loading the open project says OPEN ALREADY.
- FILE CHANGED is only an expiring header alert: no lasting cue the project isn't on disk.
- SLOT OFF vs the recorder (bug 5): refuse, or take the next free slot?
- POOL doesn't mark missing samples (looks loaded, minus the length).

## Cosmetic

- The NEW pop-up's header says LOADING; the cat also plays for failed loads.
- The keybed header says RENAME without the project's name.
- The keybed's white-key letters are packed edge to edge.
- SAMPLES' path message: BREAKS/ going in, /AUDIO/BREAKS going up, AUDIO at the root.
- Broken files' cards: unreadable shows "0 BPM 0 SAMPLES" instead of DAMAGED; truncated looks normal.
- ec_projects.c's header comment lists the old key map.

## Documentation mismatches (FM1-CONTROLS.md unless noted)

- Panel table: FILE as "FILE -> SAMPLER", hold = recorder's setup; line 36 "FILE is always BROWSE". Real: SAMPLES >
  PROJECTS > EDITOR > SLICE > SAMPLER; hold = the mode picker.
- Line 44 "project load: LOOP ENGINE" (since 0.6.46 a load stays in PROJECTS).
- FILE section: BROWSE (B3) / POOL (B6) pages, "SEL turns the page", "SEL leaves to BROWSE" (pre-0.6.42).
- POOL table: F1 MOVE and B11 preview; code: F1 says HOLD B11 TO MOVE, B11 held moves (ec_panel.c:528-532).
  UX-V2.md:460 the same.
- Sample pool: FREE "F2 in FILE's browser"; it is B9 in SAMPLES.
- Projects: files named PROJ-001 / .PROJ-001.ecp; since 0.6.47 they take the project's name.
- DELETE of the open project: docs say EDITED; it is re-saved (bug 4).
- UX-V2 §32 "Autosaves keep refusing" is not true (bug 1).
- PROJECTS-DESIGN.md shows the unbuilt key map (B10 LOAD, B9 SAVE, B8 SAVE +1, B3 RANDOM, newest first), no
  "superseded" note.
- Name length: docs say 14, but +1 and taken-name numbers can make 16 (SAUCEY FURBALL 2), which the keybed scrolls.

## Checked and fine

- Rich-project round trips: power-on load -> SHIFT+SAVE gives the same bytes except the header BPM (follows the
  current pattern) and the checksum (s02); same through a PROJECTS load + SAVE AS +1 (s24); RENAME of another
  project is byte-identical (s24); a missing sample keeps its slot through load -> save (s16).
- --flash power cycle: PITCH, TRIG steps, EDITOR trim come back (s12a/b). Autosave 2 s after the last edit or a
  stop; SHIFT+SAVE works while playing (s17).
- LOAD with edits asks; no / yes / OPEN ALREADY right; LOAD and NEW while playing keep the transport, no crash (s14).
  OVERWRITE works, the old file untouched (s15). SAVE AS +1 numbering right, cursor follows (s03). NEW stays in
  PROJECTS with the cat (s03).
- Natural sort; dot files, .txt, a folder named sub.ecp ignored (s18). A damaged file with a whole hidden copy loads
  as "(COPY)" (s18).
- Keybed: RENAME of the open project renames the file, deletes the old, updates last.txt (s08); typing replaces the
  selection; 14 limit; rolling and history on K4, PRESETS, K2; OCT- clear / cancel, SHIFT+OCT- clear, OK on empty
  keeps the old, HOME cancels, FILE = OK; a taken name gets " 3"; SEL at the end adds a character (s10, s19).
- SAMPLES: AUDIO/ root, UP row, OCT+ / OCT-; B11 click, B11 hold preview, B10 LOAD, B9 FREE; FREE, take-out, purge
  say STOP FIRST while playing (s11, s20).
- POOL: hold-B11 MOVE, ASSIGN, F2 FREE (s23).
- SAMPLER: a 1-bar take lands in the next free slot; rec numbers skip existing files; takes reload after power-on
  (s13).
- 0.6.15 bug 11 (EDITOR turns merged into one undo) stays fixed (s22).
- Fresh boot: T1 on slot 1, T2-T6 OFF.
