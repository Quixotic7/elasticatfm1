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
| FX | FX: the insert machine's params (a visual per machine) / SENDS: S1, S2 levels, TAP. On S1 / S2 / MST: that bus's FX machine | next page | setup: **the FX machine** (insert, or the selected bus's), send tap |
| LFO | MOD: LFO 1 / LFO 2 / MOD ENV / VELOCITY / MACROS / MACRO 1-4 (value + depth to PTCH CUT RES AMP PAN) | next page | - |
| GLO | MASTER: MIX (meters + faders) / MASTER (BPM, swing, global length, playhead return) | next page | setup |
| SAVE | FILE: the sample browser | **loads the file under the cursor into the track's slot** | - |
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
| S1, S2 | Select the send bus: its FX page (any category but MST / FILE / PAT) | |
| MST | Select the master bus: SRC shows the MIX, the other categories the master insert | |
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

## E4: FX, sends, MOD

- **18 FX machines** (NONE, DELAY, REVERB, LOFI, COMP, DESTROY, ECHO, BLACKHOLE, CHORUS, FLANGER, PHASER, DJ EQ, DUCK,
  TAPE ECHO, CASSETTE, MOTION, RINGS, LIMIT) on four kinds of slot: each track's insert, the S1 and S2 send returns,
  the master insert. The machine is chosen in the setup (**FN + FX**, KN1) so a knob turn never swaps it by
  accident; it can change while playing (a short fade, no click). On a send, NONE returns nothing.
- The FX page shows the machine's cells (FN for cells 5-8) over a picture: the delay taps, the reverb's decay, the
  compressor / limiter curve with the live IN and GR meters, the crushed wave, the sweep, the EQ bands; the bar at
  the right is the slot's output level.
- **SENDS** (FX, second page): S1 / S2 levels and TAP (PRE = after the filter, POST = after the insert). A muted
  track still feeds its sends (as the original). The MIX page meters the S1 / S2 returns.
- **DUCK** listens to the dry program: on a send return, the six tracks; on an insert, the tracks before it; on the
  master, the master.
- **MOD**: two LFOs (DEST, WAVE, SPD synced, DEP bipolar, MODE free / trig / one / hold), the mod envelope (ADSR,
  DEST, DEPTH; retriggered where the step's ERST is on, the LFOs where LRST is on), velocity (slice trigs) and four
  macros. DEST: PITCH (+-12 st), CUTOFF, RES, AMP, PAN, or a macro (M1-M4). The bar at the right of the LFO / env /
  velocity pages is the live value at the destination. Depths, speeds, the env times and macro values p-lock; DEST,
  WAVE and MODE do not (as the original).
- A stopped track that has gone quiet for 4 s stops using CPU until it plays again (the original's idle saver).

Where E4 differs from the original:
- The bus FX (S1, S2, MST) are global and **not p-lockable**: the original's bus *lanes* (three extra step lanes
  that lock bus params) are not ported yet.
- The macro depth matrix is edited on the MACRO 1-4 pages (the original used a grid gesture: hold a macro key and
  turn a destination).
- The FX machine list is the original's 18 (DRIVE removed): a 0.1.0 project's stored insert machine index lands on
  a different machine (none of them was audible in 0.1.0).
