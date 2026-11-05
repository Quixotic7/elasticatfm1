# QA session, Elasticat 0.6.15 (2026-11-04)

**Outcome (0.6.16, commit e9b02f2):** bugs 1, 2, 3, 6, 7, 8, 9, 11 fixed; 5 (the recorder's setup slot) kept as
E6 designed it (a resample into the track's slot; its SLOT row picks another) and documented; 4 (KEYBOARD's slice
pick with a target) kept: the black key writes the slice trig, as the E1 demo relies on; 10 (capture) was the doc's
wording. docs/UX-V2.md section 12 has the details.

Two sweeps through the headless harness (`build/ec_run`), one on the slice machines and the SLICE editor, one on
every other mode. No crashes, no asserts, no ec_run errors; `ctest`: 36/36. Scripts and screenshots: `slices/`
and `modes/` here (each `sN.txt` / `tN.txt` with its `out/`). Code checks by the session are marked (confirmed).

## Bugs (wrong behaviour)

| # | Area | What | Repro (script) |
|---|---|---|---|
| 1 | SLICE editor | Slice-table edits (EVEN, CHOP, split, delete, mute, SLCE START/END) are not on the undo stack; SHIFT+F1 skips to the earlier CNT / MACH changes. (confirmed: the undo records cover params, steps, globals and the crossfader only) | slices/s6.txt, s5.txt |
| 2 | SLICE editor | Undoing a CNT change restores the count but not the table: after COUNT 4 + EVEN then undo, slices 1-4 keep quarter widths and 5-16 keep stale 1/16 edges that overlap them. (same cause as 1) | slices/s7.txt |
| 3 | SLICE editor | Auto-Chop never slices from the first hit: a_drums (first hit at 0) gives 7 slices starting at 0.125; d_perc16 at 50 % gives 17 slices with the first hit split. (confirmed: `ec_razor_transients` scans windows 1..W-2 and needs a local peak, so an onset in window 0 is never found; a slice from 0 to the first transient is never made) | slices/s17.txt, s16.txt |
| 4 | KEYBOARD, slice track | With a target step, B1-B10 (slice pick) also write a trig and advance the step, so a slice cannot be picked and then played for the same step. | slices/s9.txt from 12000 |
| 5 | SAMPLER | The recorder's setup (REC held) has SLOT = POOL 01 by default; F2, F2 records over slot 1 (a_drums replaced by the take, no undo). B11 in the SAMPLER takes the next free slot as documented. | modes/t13.txt |
| 6 | MOD | F2 + knob (MOD CLEARED) leaves no undo entry; SHIFT+F1 says UNDO DPTH but the route stays gone, redo does not restore it. | modes/t21.txt |
| 7 | Panel | Letting go of a held menu button without picking a black key still cycles / opens the mode (with and without MODE LOCK). The owner's spec: a hold with no key changes nothing. (confirmed: `button_up` calls `mode_tap` whenever `btn_used` is 0, whatever the hold's length) | modes/t22.txt, t23.txt |
| 8 | MODE LOCK | With FILE locked to the EDITOR, FILE opens the EDITOR while playing (the editors are meant to need a stop). | modes/t31.txt |
| 9 | PAGES | "PAGE 1 MUTED" is said when page 1 is the only page in the loop, but nothing is muted. | modes/t16.txt |
| 10 | CROSSFADER | B11 held + B1 says SCENE A CAPTURED but A LOCKS stays 1 (`sa` stays 1); the docs say capture takes every morphable value. | modes/t19.txt |
| 11 | EDITOR | A K1 turn and a K2 turn 600 ms apart merge into one UNDO TRIM; undoing a trim also discards the STEPS that RECALC set. | modes/t11.txt |
| 12 | LOOP SETUP | The machine changes while playing (only WARP MODE shows the red STOP); the demo scripts and FM1-CONTROLS say the machine changes only while stopped. Decide which is intended. | slices/s2.txt from 26000 |

## Design questions

- SLICE editor: turning COUNT down without EVEN keeps the old edges, so COUNT 16 -> 4 leaves four 1/16-wide slices
  over the first quarter and the rest of the sample unreachable (slices/s6.txt). If COUNT is meant as "how many
  slices exist", lowering it should probably redistribute or at least stretch the last slice.
- KEYBOARD on a slice track: B11 steps the bank to "SLICES 11-20" even with 16 slices (slices/s9 04_bank.png); the
  editor's white keys follow the bank of the ALGO-selected slice, which is undocumented (slices/s15).
- MIX B9/B10 step a fine rate list (1/16 .. 7/8, 5/6 .. 11/6, 2, 4) rather than the B1-B8 list.

## Cosmetic

- RAZOR / slice tracks: after a 1SHOT pad ends, `voices` stays 1 for ~3 s and the playhead and slice highlight stay
  on the LOOP page for 13 s+; a stale orange line also sits in the editor view (slices/s14, s13a, s15).
- CROSSFADER: the big number and the POSITION cell disagree by one (104/105, 127/128; 128 is outside 0..127)
  (modes/t19).
- F3 screen: "120.050%" (no gap between the tempo and the swing) (modes/t4 f3_zoom.png).
- PATTERN: a cut source (F2 CUT A3, paste on A5) still draws as used and `patterns` stays 3 (modes/t18).
- CAPTURE record mode's header says "REC: PLAY TO START" although the first key starts playback (modes/t28).

## Documentation (docs/FM1-CONTROLS.md still says 0.6.10 in places)

- SHIFT+HOME is listed as APP SETTINGS (now LOOP SETUP; APP SETTINGS is SHIFT+ARP); "hold" opens settings and hold
  SEQ = F3 (a hold is now the mode picker); FILE held = the recorder's setup (now REC held in the SAMPLER).
- The RAZOR section (around lines 1108-1120): knob layout, "SEL: back to EDITOR, on to POOL", no B9 EVEN / B10
  CHOP / B1-B3 pages / B11 preview, and the page is called RAZOR where the screen says FILE/SLICE.
- EDITOR: "SEL leaves for BROWSE" (it stays on the editor's pages); "the steps follow the trim" (only RECALC does).
- PAGES "K3 LOOP" knob (not shown); memorize / recall given as SHIFT+B3/B4 in two places (it is SHIFT+B5/B4); POOL
  "can't open while playing" (it can).
- PATTERN layout table says K1 QUANT, K2 LENGTH; the screen shows PLAYING, NEXT, QUANT, LENGTH and K1/K2 do nothing.
- ui_info has no `machine:` field, `bank:` stays 0 while KEYBOARD shows a bank, `ppos` is 0 on slice machines
  (harness only).

## What worked

SLICE / RAZOR / S.POLY / R.POLY from LOOP SETUP and the B2 menu; velocity pads stopped and playing, mono takeover,
8 poly voices; CNT, PLAY modes, SREV, RTCH, SYNC / RATE; EVEN 1..32, CHOP, split, delete, mute, edges honoured by
the pads, zoom on every page, B11 preview stopping on release, previews on non-slice machines; SLIC / RTCH locks and
COND ratchets; live REC of pads and keys; save, reload, autosave, power-on reload, memorize / recall; TRIG steps and
every lock, COND (chance, 1:2, ratchets, FILL), PLAYHEAD page, step copy / cut / paste with named undo; TRIG machine
LEN; NEIGH and INPUT; EDITOR markers, zoom, key placing, AUTO BPM tagged and untagged, RECALC; SAMPLER lengths, bar
sync, takes in data/elasticat/rec; MIX mute / solo / CAT / PRESETS; PAGES loop and copy; PATTERN launch, queue,
copy, naming; CROSSFADER scenes, glide, jump, clear; FX menu, SENDS PRE / POST, the sends move; MOD routes; APP
SETTINGS THEME, F1 revert, MODE LOCK.
