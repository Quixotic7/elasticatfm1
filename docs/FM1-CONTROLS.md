# Elasticat on the FM-1: controls

The FM-1 is the panel; the sound runs in the RemoteFM1 host. Samples come from your `RemoteFM1/audio/` folder.
**FN** is the HOME button (hold it). Six tracks **T1-T6**, each with its own sample, machine, warp, filter, amp and
16-step pages; 16 patterns. A new project loads the first file in `/audio` onto T1.

## Knobs

| Knob | Does |
|---|---|
| MASTER | Master volume |
| SELECT | Page within the category (on FILE: moves the file cursor) |
| PRESETS | Pattern -/+ (queued to the next bar, see PAT) |
| ALGORITHM | Track: T1..T6, then S1, S2, MST |
| KN1-KN4 | The page's cells 1-4. **Hold FN: cells 5-8** (the orange tick at the right of the cell grid shows which bank) |

Hold a step + turn a knob = **p-lock** that cell on the step (the value turns orange). Hold step(s) + **PLAY** = clear
their p-locks (the trig stays).

## Buttons

| Button | Category | Tap again | FN + tap |
|---|---|---|---|
| SEQ | TRIG: NOTE LEN VEL, resets / COND: chance, condition, ratchet / MTRIG or SLICE | next page | swing |
| SEL | SRC: SOURCE / MACHINE / WARP / RANGE (acts on release) | next page | setup: machine, warp, loop division, poly |
| EDIT | FILT: FILTER (curve) / F.ENV / F.MIX | next page | setup: filter machine, filter env mode, mix mode |
| ENV | AMP: envelope, VOL, PAN, sends | - | setup: env mode, env range, portamento, slice hold |
| FX | FX, SENDS (stored; heard from E4) | next page | - |
| LFO | MOD: LFO 1, LFO 2 (stored; heard from E4) | next page | - |
| GLO | MASTER: MIX (meters + faders) / MASTER (BPM, swing, global length, playhead return) | next page | setup |
| SAVE | FILE: the sample browser | **loads the file under the cursor into the track's slot** (on RAZOR: back to the browser) | **RAZOR** page (E3) |
| ARP | PAT: length, rate, global length, change mode | - | setup |
| PLAY | Play / pause (resume keeps the position) | | **stop + back to step 1** (twice: all voices off) |
| REC | (E5: copy / paste) | | **save the project now** |
| OCT- / OCT+ | Lower keys: STEPS / LOOP KEYS / KEYS (OCT- lit = loop keys latched, OCT+ lit = keyboard) | | step page -/+ (patterns longer than 16) |

In a setup list, FN + the same button goes back to the category's pages; another button jumps to its setup.
Warp mode, filter machine and filter mix mode change only while stopped (the list says STOP).

## Upper keys

| Key | Does | FN + key |
|---|---|---|
| T1-T6 | Select the track (steps, pages and knobs follow) | Mute / unmute |
| S1, S2 | Select the send bus (placeholder until E4) | |
| MST | Select the master bus (SRC shows the MIX) | |
| FILL | Hold = fill (FILL / !FILL trig conditions) | Latch fill on / off |
| PATN | Hold = pattern overlay; tap = latch it open. Lower keys 1-16 pick the pattern | |

## Lower keys (16)

**STEPS** (default): quick tap = trig on / off (a new trig keeps the key's velocity); **FN + tap = ghost trig**
(plays nothing new, carries the last note's locks). Hold a step + turn a knob = p-lock.

**Hold SEL = loop keys 1-16** (the live scrub): hold one key to loop that slice of the region, two or more to span
them; let go and the playhead returns per MASTER > RTN (return / boomerang / reset). While stopped they audition.
On slice machines (SLICE, S.POLY) the keys are **slice pads** 1-16, velocity sensitive.

**Hold a step, then hold SEL and press loop keys** = lock that step's region: first tap start + end, the same key again
start only (end follows the track), two keys = their span. On slice machines it picks the step's slice(s) (SLICE:
one, S.POLY: several).

**KEYS** (OCT+): a white-key keyboard, -12..+14 semitones, centre C on key 8. Slices play the track's slice at that
pitch; loops transpose while held. Hold a step + a key = lock the step's pitch.

## LEDs

Steps: trig = mid, p-locked trig = flicker, ghost = dim, playhead = bright. Loop keys: the region dim, the playhead's
key mid, held keys bright. Upper keys: the selected track bright, the others by their level, muted off. Category
buttons: the current one lit (blinking in setup). PLAY green while playing.

## Where the FM-1 differs from the grid

- The grid's separate rows (loop keys, steps, keyboard, pattern slots) share the 16 lower keys: SEL held, the OCT key
  modes and the PATN overlay switch them.
- STOP is FN + PLAY; the grid's PAGE key is FN + OCT-/OCT+; CUT/PASTE, copy / clear / paste (REC / PLAY / STOP with a
  scope) wait for E5, except **hold step + PLAY = clear its locks**.
- Loop keys 17-32 (loop division above 16) are not reachable; slice pads reach slices 1-16.
- The norns K2/K3 pair navigation is replaced by four knobs on four cells (FN for the next four).
- Pattern rename, scenes / crossfader, macros, undo (NO), the visualizer: not yet (E5).

## FILE / RAZOR: the razor slice editor (E3)

**FN + SAVE** on the FILE browser opens page 2, **RAZOR**; SAVE goes back. The razor machines (**RAZOR** = mono,
**R.POLY** = poly, up to 8 voices, as S.POLY) play each slice from its own start / end; pick them in the SRC setup list
(FN + SEL, KN1). The table is per track and saved with the project; the per-slice settings (PLAY, CHOK, P/T, mute)
apply to every slice machine.

| Control | Does |
|---|---|
| Lower keys 1-16 | Select that slice and play it (velocity); slices 17-32 when the selected slice is in that bank |
| FN + lower key | Mute / unmute the slice (it stays on its steps, plays nothing) |
| SELECT | Select the slice |
| KN1 / KN2 | **STRT / END** of the selected slice (0..1 of the track window), under the SNAP mode |
| KN3 | **PLAY**: the slice's play mode (TRACK = the track's) |
| KN4 | **ZOOM** 1X..64X, centred on the point you last moved |
| FN + KN1 | **SNAP**: GRID (next slice_count division), ZOOM (0.5 / zoom per detent, the plain mode), ZERO-X (lands on the nearest zero crossing), TRANS (jumps to the next / previous transient), FRIEND (also drags the touching neighbour point) |
| FN + KN2 | **CHOK**: the slice's choke group A-H (a new voice in the group cuts the others; OFF) |
| FN + KN3 | **P/T**: the slice's pitch offset |
| FN + KN4 | **SENS**: Auto-Chop / transient sensitivity (the ticks over the waveform) |
| OCT+ | **Split** the selected slice at the point you last moved (else in the middle) |
| OCT- | **Delete** the selected slice |
| FN + OCT+ | **Auto-Chop**: one slice per transient (the head before the first one left out), the slice count follows |
| FN + OCT- | **Redistribute**: the slices in use as even divisions |

Split and delete renumber the slices after it; the track's steps follow (a step on slice 6 stays on that audio).
The screen: the waveform of the track window (trim x Range), the selected slice as a cream block, slice numbers,
the transients at SENS as ticks, the sounding voices in orange, the zoom window as a bar, the 16 pads of the bank.

## E3: warp modes, the neighbor machine

Every warp mode plays now (SRC setup, KN2, while stopped). The WARP page (SRC page 3) shows each mode's params as
the original's mode layout: DOMINO SLCS SLEN LOOP RATE / ATK REL GATE; RANDOM OLA CHAOS OWIN OWAN RATE; PITCH CORRECT
SMEAR PCWN PCDS RATE; HARMONIZER MIX IN1 IN2 IN3 / RATE (an interval of 0 = that voice off); WAVETABLE MORF WSIZ
CYCW LRAT / LDEP LSHP XFAD RATE (pitch from P/T, base C3; KEYS mode plays it); SPECTRAL FREEZE FRZE BLUR RATE;
FORMANT FORM FRZE RATE; GSTRETCH / GSTRETCH 2 STRCH PWIN SMTH RATE. CYCW and LRAT move in semitone ratios.

**NEIGH** (SRC setup machine, tracks T2-T6): the track's source is the track to its left, whole (after its filter,
amp, pan, mute and, from E4, insert). The left track is then heard only through this one: this track's filter, amp
and pan shape it, its mute silences both. SRC shows the routing and both levels. Chains work (T2 into T3 into T4).

Deviations from the original in E3 (the razor part; the engine's are in src/dsp/README.md): the razor table is per
track (the original shared one global table); the plain STRT / END turn is the ZOOM snap (0.5 / zoom per detent,
the original's plain detent at 1X) and the other snaps act on every turn instead of FN + turn; split / delete /
per-slice play mode + pitch are added (the original had per-slice locks, choke and mute); the onset envelope reads
32 frames per window (the Lua 8).
