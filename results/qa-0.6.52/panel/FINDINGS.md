# QA 0.6.52: tracks and panel (2026-11-06)

30 scripted scenarios on fc47945: no crash, assert or ec_run error. A 4-minute stress run (6 tracks + sends, ~400
random inputs) ran at 154x real time, no drift, silent after stop. Scripts sNN*.txt / pNN.txt with out/NAME/;
run.sh (fresh folder copy), f.sh (ui_info fields), probe.c / probe (built against build/libelasticat.a and
host/ec_harness.h: dumps live labels, LEDs, a step's locks), prun.sh.

## Bugs

| # | Sev | What | Repro (script @ ms) | Code |
|---|---|---|---|---|
| 1 | Med | MIX: PRESETS walks T1..T6 -> S1, then is stuck on S1 (no turn moves it) | s01 @11300-12300 | presets() returns early on A->bus (ec_panel.c:1016) before the MIX branch |
| 2 | Med | MIX with S1/S2 (or MST on P1/P2) shows T1's LEVEL / PAN / SEND cells and labels; the knobs do nothing (no change, no undo) | s03b @7000-7500, 8700 | ec_app.c:195 page from the track's values; ec_app.c:753 returns on A->bus |
| 3 | Med | The edited step page is never clamped to the length: after LEN goes down (PAGES K1, B11+ALGO, undo, RECALL) or a shorter track is selected, the header says page 2 and a white key writes an invisible trig past the end (step 17/19), heard if the length grows | s05b @7300-10300; s14 @7200-8500; s05 @19000 | only step_page_turn clamps (ec_panel.c:995); not ec_pn_select_target (:364), LEN edits, undo, recall |
| 4 | Med | Pattern cut / paste have no undo: paste overwrites a used pattern silently; SHIFT+F1 then undoes an older edit (UNDO LOOP). Cutting the playing pattern silences it, no way back | s04 @16300, 20100, 21100 | ec_clip.c:73 / :95 record nothing |
| 5 | Med | KEYBOARD: after advancing (target 1 -> 3), ALGO +1 goes to 2, ALGO -1 to LIVE: ALGO counts from the old selection, so entry goes backwards and overwrites | s10b @7600-8000 | enter_step moves only P->target (ec_keyboard.c:80); ec_lp_algo uses ec_lp_first() (ec_loopstep.c:139) |
| 6 | Med | PAGES: a span whose second key is B9-B11 is ignored and the first key's hold mutes its page (B2 held + B10 -> PAGE 2 MUTED); B10 held + B2 works | s05 @12200-12900 | others && i < 8 (ec_mix.c:153) doesn't mark the held keys used; the hold timer (ec_mix.c:220) fires |
| 7 | Low-Med | Scene clear (SHIFT+B1/B2), capture and B1/B2 + knob edits have no undo; clear has no confirm | s06 @13600-15200 | ec_xfd.c:83, :90; ec_e5.c:687 |
| 8 | Low-Med | Page-loop changes have no undo (hold, double-tap solo, span, F3 held page); undoing a span that grew the track restores LEN but leaves a wrong mask (ploop 0006) | s05 @13900-14700 | ec_mix.c:123 / :233; ec_e5seq.c:243 / :252 |
| 9 | Low | MIX hold-mute and SHIFT+B11+key ALL TRACKS ON have no undo (the K1 MUTE knob does) | s02 @11500 vs s03 @6800 | ec_app.c:1338 |
| 10 | Low | KEYBOARD: a vel-100 note over a vel-40 note keeps the 0.31 velocity lock | p03 (step@7600 vel:0.314961) | ec_keyboard.c:73 sets a lock when vel != 100, never clears |
| 11 | Low | REC tap arms live recording in PROJECTS and the SAMPLER (REC: PLAY TO START); docs: REC does nothing in FILE | s11 @10600, 12300 | ec_panel.c:573 guards EC_CAT_FILE only; :160 |
| 12 | Low | Live REC makes one undo entry per recorded step (one pass: 8), so a bar of automation pushes everything out of the 16-deep stack | s11 @6300-9400 | ec_liverec.c:134 keys the undo by step |
| 13 | Low | B11-held layer labels: ALGO says STEP, PRESETS PAGE, but they set LENGTH and ZOOM. PAGES: K3 says ZOOM and does nothing; ALGO says STEP but turns the page | p02, p01 @10600, s05b @8800 | ec_panel_leds.c:607-610 |
| 14 | Low | MIX, CROSSFADER, PATTERN: ALGO labelled STEP PG (turns a hidden step page, PAGE 1/1); CROSSFADER and PATTERN: PRESETS labelled SLOT and changes the track's sample | p01 @9600-12600 | ec_panel_leds.c:642-644; ec_panel.c:1254 |

## Design questions

- Output clipping: a hard clamp, no limiter (ec_app.c:1504); 6 default tracks with sends clipped 0.7 % of samples, up
  to 50k per 20 s window (s12).
- Lost detents: lists, MIX PRESETS, SHIFT+PRESETS move one step per tick however many detents arrive (s09 @13600,
  s01).
- MIX B9/B10 still walk the fine 22-entry rate list (open since 0.6.15); rate presses < 1 s apart merge into one
  UNDO RATE (s02).
- Sticky F1 on LOOP ENGINE: header says COPY, but the white keys still edit the loop (s15 @8500).
- Sticky F1/F2 survive mode changes; sticky SHIFT doesn't. Intended?
- Cutting the playing pattern leaves silence with no undo (bug 4).
- LOOP START can reach END (128/128) with KNOB SPEED FAST (s16_2).
- Capture stores only off-default values or ones the other scene holds (A LOCKS 1); docs: "every morphable param".
- LOOP SETUP opened with sticky SHIFT + HOME stays open under everything (s08 8600-14400); the next F1 closed it
  instead of undoing.

## Cosmetic

- Still open from 0.6.15: CROSSFADER 127 vs POSITION 128; F3 "121.053%"; a cut pattern drawn as used and counted;
  CAPTURE header "REC: PLAY TO START" (cut "REC: PLAY TO ST.." in PROJECTS).
- PAGES: the last page draws 16 squares whatever the length (LEN 39, 13).
- MIX header's right box: "P01" with a bus, "1" with a track.
- Labels cut at 10 characters ("APP SETTIN", "SNAP PLAYI"); PATTERN's SHIFT layer shows SNAP on K1-K4 that do
  nothing.
- SHIFT LEDs: LFO lit (96) with no settings; B4 RECALL dim (40) with nothing memorized.
- Macros page: M1-M4 names clipped by the knob area (s12 r115682.png).
- ui_info (harness): clip: twice; LOOP SETUP reports cat:LOOP; voices stays 0.

## Documentation mismatches (FM1-CONTROLS.md)

- Title "(0.6.32)". Panel table: Hold 0.4 s column lists settings and SEQ = F3 (a hold is the mode picker, 300 ms);
  FILE as "FILE -> SAMPLER" (now SAMPLES -> PROJECTS -> EDITOR -> SLICE -> SAMPLER).
- Header table: FX SETUP etc. while held; the header says PICK A MODE.
- PATTERN LAUNCH: "Hold SEQ = F3" (does nothing, s07 @11600); knob list vs layout table.
- PAGES: knobs LENGTH, RATE, LOOP (actually LEN, RATE, an idle K3 ZOOM); double-tap solo, spans, B1-B8 limit only
  documented for B11 + LOOP ENGINE.
- PRESETS turns the step page on every track page, PAGES, KEYBOARD, not just LOOP ENGINE and TRIG.
- CROSSFADER "W12 = 127" (reaches 128).
- Live REC "every mode but FILE" (bug 11).
- APP SETTINGS titled SHIFT+HOME (opens with SHIFT+ARP); rows table lacks MODE LOCK; "K1-K4 change the four rows"
  (only K1 changes the selected row).
- F3: a held page key's header says "PAGE n", not "LOOP PAGE n".
- MIX: nothing says what the cells do with S1/S2.

## Re-checked from qa-0.6.15

Still fixed: #7 (a hold with no key changes nothing), #8 (MODE LOCK FILE->EDITOR while playing: STOP FIRST, BROWSE),
#9 (the only page: LAST PAGE). The cosmetic items above are still open.

## Checked and fine

- Target selection: SHIFT+PRESETS walks and clamps; SHIFT+W1-W9 select.
- MIX: select, hold-mute, solo, clear-all, B1-B8 rates, B9/B10 clamping, the CAT (track keys work under it), MST lands
  on MASTER, GLO LENGTH with undo.
- PATTERN: copy / paste, launch stopped and playing (queued, switched on the boundary), banks.
- PAGES: tap, hold-out, spans that grow the track, page copy, memorize / recall.
- CROSSFADER: scene B + knob edits, glide, SHIFT+W jump, K1 with undo, W13-W16 glide times, capture, clear.
- F3: every control; every edit undoable in order.
- SHIFT: sticky, used-hold, sticky ending on a mode change, tap tempo (150.0, one undo), latch, undo / redo with
  sticky SHIFT.
- Mode buttons: cycles, picker, MODE LOCK. APP SETTINGS: F1 reverts, F2 keeps; KNOB SPEED takes effect.
- KEYBOARD: octave +-3 with LEDs, note entry and undo, key labels.
- Live REC: OVERDUB writes and stops on PLAY; REC hold opens the settings.
- Copy / paste of steps across tracks with sticky F1.
- Stop / start spam: 41 PLAYs 37 ms apart, PLAY+REC, PLAY during launches: no stuck state, 279x real time.
- Labels / LEDs right per mode apart from bugs 13-14.
