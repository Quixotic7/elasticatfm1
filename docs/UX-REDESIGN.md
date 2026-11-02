# Elasticat FM-1: UX redesign along ChoralRoot and FIZZ (proposal, 2026-11-02)

The owner asked for Elasticat's button and knob interactions to work like ChoralRoot's and FIZZ's. Sources:
ChoralRoot `docs/MANUAL.md` §3 (the panel), §12 (the sound editor), §18 (quick reference); FIZZ `PLAYING.md` ("Names
used here", "Getting around", track settings, mixer, effects, F3, SHIFT's keys); the owner's rules: single clicks over
holds, SHIFT + button for shortcuts, no tutorial text, nothing under ~12 px, strong visuals.

## What the two firmwares share (the rules to adopt)

1. **GLO is SHIFT.** Held with a knob: the value snaps to useful values (ChoralRoot's OPT snap; Elasticat's original
   K1 + E2/E3 did the same). Held with a button or key: that key's shortcut. While held, only the shortcut keys light.
2. **A category button opens its page; tapping it again goes to its next page.** The button is lit while its page
   shows.
3. **Lanes, not modifiers, for more than four values.** A page has one or more lanes of four values on KNOB 1..4,
   drawn in the knob colours; the other lanes are grey. **SELECT steps the lanes** and runs on into the next page, so
   SELECT alone reaches everything in a category (ChoralRoot's editor).
4. **HOME = back to the main view, from anywhere, in one press** (closes any pop-up, keeps your place in each
   category).
5. **Pop-ups stay open on release** (single click). In a pop-up **OCT- = back / cancel, OCT+ = OK / confirm**; HOME
   closes. Questions ("CLEAR?") are answered with OCT+, never with a hold.
6. **OCT- + OCT+ together = F3**, the transport/timing menu (tempo, swing, pattern length/rate, page select / loop);
   **holding PLAY** opens it too (FIZZ).
7. **Knobs accelerate** (slow: finest step; spun: up to ~1/16 of the range a click); lists are always one item a
   click.
8. **A held step owns the knobs**: the screen gets a frame in the lock colour, every turned value becomes that step's
   lock (a filled chip; outlined = locked on some step), **OCT+ held + a knob clears that lock** (FIZZ F2), the count
   of held steps shows top right. Nothing held: the knobs move the track.
9. **Track keys** (on the mixer and with SHIFT): tap = select, hold = mute, double-tap = solo; one key clears all
   (FIZZ mixer).
10. **A knob turn on a page that doesn't show the value** takes over the screen for ~1 s: the value big, a stripe
    meter, its name (ChoralRoot).

## Elasticat with those rules

| Control | Now (0.2.0) | Proposed |
|---|---|---|
| GLO | FN (hold): cells 5-8, setups, alt functions | **SHIFT**: snap, shortcuts (table below); no longer needed for cells 5-8 |
| HOME | MST category | **back to the main view** (SRC/WARP with the waveform) |
| FX SEL ENV LFO EDIT SAVE ARP SEQ | FX SRC AMP MOD FILT FILE PAT TRIG; tap again = next page; FN + = setup | same categories; tap again = next page; **setup: SHIFT + the button, or hold the button** (400 ms; stays open); a machine/warp/filter change follows the original's stopped rule |
| SELECT | page | **lane** (runs on into the next page) |
| KN1-KN4 | cells 1-4 (FN: 5-8) | **the active lane's four values** |
| ALGORITHM | track | track (kept) |
| PRESETS | pattern | **the track's sample** (next / previous file of the folder, heard at once, like a preset); SHIFT + PRESETS: the pattern |
| MASTER | volume | volume |
| OCT- / OCT+ tap | lower-key mode cycle | outside pop-ups: **the lower keys' mode** (OCT-: STEPS ↔ LOOP KEYS, OCT+: STEPS ↔ KEYS; both lit = SCENE as now); in pop-ups: back / OK |
| OCT held | PAGE overlay | (moves to F3) |
| OCT- + OCT+ | - | **F3**: TEMPO (SELECT), SWING (ALGORITHM), pattern LEN / RATE (KN1 / KN2), page select / loop on the lower keys |
| PLAY | play/pause; FN + = stop | tap play/pause; **hold: F3**; SHIFT + PLAY: stop |
| REC | scope + REC copy; FN + save | scope + REC copy (Elektron, as now); **SHIFT + REC: save** |
| SRC (SEL) held | loop keys | **loop keys** (kept: Elasticat's signature, a momentary performance gesture) |
| Upper keys | T1-T6 S1 S2 MST FILL PATN | same; **hold a track key = mute, double-tap = solo** everywhere; MST key = the MIX page (it was HOME) |
| LFO / MOD | MOD category | MOD category |

### SHIFT's keys (lit while SHIFT is held)

| SHIFT + | does |
|---|---|
| OCT- / OCT+ | **undo / redo** |
| upper T1..T6 | **memorize / recall** stay on FN+OCT today → move here: T1 memorize, T2 recall (shown as icons) |
| upper S1 / S2 | copy / paste the pattern |
| upper MST | tap tempo |
| FILL | latch fill |
| PATN | name the pattern |
| PRESETS | the pattern |
| any value knob | snap |
| PLAY | stop + back to step 1 |
| REC | save the project |

### Screens

- Every page: the header (track, CATEGORY / PAGE, lane dots), the picture, the four cells of the active lane in the
  knob colours (KN1..KN4 tags as coloured chips); other lanes' cells shown small and grey only where room allows.
- The lock frame and chips (rule 8) replace the orange value text for locks.
- Pop-ups (F3, PAGE, PATN, naming, questions) show OCT- / OCT+ as two small arrows top right, never text hints.

## Owner's decisions (2026-11-02)

1. **Keep the cream + orange look** (no knob colours): the active lane is bright cream, other lanes grey; locks orange.
2. **ALGORITHM stays the track knob** (no step cursor).
3. **PRESETS = the track's sample** (next / previous file of the folder, heard at once); SHIFT + PRESETS = the pattern.
4. **Setup = SHIFT + category, or hold the category button** (a hold opens it at 400 ms; it stays open on release).
   SRC is special: held with a lower key it is the loop keys; held 400 ms and released with no lower key used, it
   opens the SRC setup.
