# Elasticat on the FM-1: controls (0.2.0)

The FM-1 is the panel; the sound runs in the RemoteFM1 host. Samples come from your `RemoteFM1/audio/` folder.
**FN** is the GLO button (hold it). Six tracks **T1-T6**, each with its own sample, machine, warp mode, filter, amp,
insert FX, sends, MOD and 16-step pages; two send buses **S1**, **S2** and the master insert **MST**; 16 patterns; two
scenes **A / B** with a crossfader. A new project loads the first file in `/audio` onto T1.

## Knobs

| Knob | Does |
|---|---|
| MASTER | Master volume |
| SELECT | Page within the category (FILE: the file cursor; RAZOR: the selected slice; naming: the place; PAGE overlay: its mode) |
| PRESETS | Pattern -/+ (queued to the next bar, see PAT) |
| ALGORITHM | Track: T1..T6, then S1, S2, MST |
| KN1-KN4 | The page's cells 1-4. **Hold FN: cells 5-8** (the orange tick at the right of the cell grid shows which bank) |

Hold a step + turn a knob = **p-lock** that cell on the step (the value turns orange). Hold slice pad(s) + turn = the
slice's own lock (see Slices). Hold scene A or B + turn = that param into the scene only (see Scenes).

## Buttons

| Button | Category | Tap again | FN + tap |
|---|---|---|---|
| SEQ | TRIG: NOTE LEN VEL, resets / COND: chance, condition, ratchet / MTRIG or SLICE | next page | setup: swing |
| SEL | SRC: SOURCE / MACHINE / WARP / RANGE (acts on release) | next page | setup: machine, warp, loop division, poly |
| EDIT | FILT: FILTER (curve) / F.ENV / F.MIX | next page | setup: filter machine, filter env mode, mix mode |
| ENV | AMP: envelope, VOL, PAN | - | setup: env mode, env range, portamento, slice hold |
| FX | FX: the insert machine's params (a picture per machine) / SENDS: S1, S2 levels, TAP. On S1 / S2 / MST: that bus's machine | next page | setup: **the FX machine** (insert, or the selected bus's), send tap |
| LFO | MOD: LFO 1 / LFO 2 / MOD ENV / VELOCITY / MACROS / MACRO 1-4 (value + depth to PTCH CUT RES AMP PAN) | next page | - |
| HOME | MASTER: MIX (meters + faders) / MASTER (BPM, swing, global length, playhead return) / SCENE | next page | setup: RTN, BPM, GBPM, NAME |
| SAVE | FILE: page 1 SAMPLE (the browser) | **SAMPLE: loads the file under the cursor** into the track's slot; when the cursor is already on the track's file: page 2 **RAZOR**. RAZOR: back to SAMPLE | **UNDO** |
| ARP | PAT: length, rate, global length, change mode | - | setup |
| PLAY | Play / pause (resume keeps the position). With a scope held = **CLEAR** | | **stop + back to step 1** (twice: all voices off). With a scope held = **PASTE** |
| REC | With a scope held = **COPY** | | **save the project now** |
| OCT- / OCT+ | Tap (on release): the lower keys STEPS / LOOP KEYS / KEYS / SCENE (OCT- lit = loop keys, OCT+ lit = keyboard, both = scene). **Hold = PAGE** overlay. On RAZOR: OCT+ split, OCT- delete | | OCT+ **memorize**, OCT- **recall**. On RAZOR: OCT+ Auto-Chop, OCT- redistribute |

In a setup list, FN + the same button goes back to the category's pages; another button jumps to its setup.
Warp mode, filter machine and filter mix mode change only while stopped (the list says STOP). An FX machine changes
any time (a short fade, no click).

## Upper keys

| Key | Does | FN + key |
|---|---|---|
| T1-T6 | Select the track (steps, pages and knobs follow) | Mute / unmute |
| S1, S2 | Select the send bus: its FX page (any category but MST / FILE / PAT) | |
| MST | Select the master bus: SRC shows the MIX, the other categories the master insert | |
| FILL | Hold = fill (FILL / !FILL trig conditions) | Latch fill on / off |
| PATN | Hold = pattern overlay (+ REC / PLAY / FN + PLAY = the pattern scope); tap = latch it open. Lower keys 1-16 pick the pattern. Overlay open + SAVE = **name the pattern** | |

## Lower keys (16)

**STEPS** (default): quick tap = trig on / off (a new trig keeps the key's velocity); **FN + tap = ghost trig**
(plays nothing new, carries the last note's locks). Hold a step + turn a knob = p-lock. Holding a trig step while
stopped plays it (step preview).

**Hold SEL = loop keys 1-16** (the live scrub): hold one key to loop that slice of the region, two or more to span
them; let go and the playhead returns per MASTER > RTN (return / boomerang / reset). While stopped they audition.
On slice machines (SLICE, S.POLY, RAZOR, R.POLY) the keys are **slice pads**, velocity sensitive. **FN + key =
17-32** when the track has them (loop division above 16, more than 16 slices).

**Hold a step, then hold SEL and press loop keys** = lock that step's region: first tap start + end, the same key again
start only (end follows the track), two keys = their span. On slice machines it picks the step's slice(s) (mono
machines: one, poly: several).

**KEYS** (OCT+): a white-key keyboard, -12..+14 semitones, centre C on key 8. Slices play the track's slice at that
pitch; loops transpose while held; WAVETABLE plays its oscillator. Hold a step + a key = lock the step's pitch.

**SCENE** (OCT- from STEPS; both OCT LEDs lit): the crossfader row, see Scenes. On MST > SCENE the lower keys are the
scene row whatever the key mode; on RAZOR they are the razor pads whatever the key mode.

## Sources: machines and warp modes

Machines (SRC setup, KN1): **LOOP**, **TRIG**, **SLICE** / **S.POLY** (even slices), **RAZOR** / **R.POLY** (the
razor table's slices, mono / up to 8 voices), **NEIGH** (T2-T6). Warp modes (SRC setup, KN2, while stopped): TAPE,
TEMPO VARISPEED, DOMINO, GRANULAR, RANDOM OLA, PITCH CORRECT, HARMONIZER, WAVETABLE, SPECTRAL FREEZE, FORMANT,
GSTRETCH, GSTRETCH 2, CHOPPED.

The WARP page (SRC page 3) shows each mode's params in the original's layout: DOMINO SLCS SLEN LOOP RATE / ATK REL
GATE; RANDOM OLA CHAOS OWIN OWAN RATE; PITCH CORRECT SMEAR PCWN PCDS RATE; HARMONIZER MIX IN1 IN2 IN3 / RATE (an
interval of 0 = that voice off); WAVETABLE MORF WSIZ CYCW LRAT / LDEP LSHP XFAD RATE (pitch from P/T, base C3);
SPECTRAL FREEZE FRZE BLUR RATE; FORMANT FORM FRZE RATE; GSTRETCH / GSTRETCH 2 STRCH PWIN SMTH RATE. CYCW and LRAT move
in semitone ratios. The spectral modes are limited to a few tracks at a time, as the original limits them.

**NEIGH**: the track's source is the track to its left, whole: after its filter, amp, insert FX, mute and pan (the
original re-points the left track's mix output). The left track is then heard only through this one: this track's
filter, insert, amp and pan shape it, its mute silences both; the left track's own sends still feed S1 / S2. SRC shows
the routing and both levels. Chains work (T2 into T3 into T4). A NEIGH track never goes idle.

## FILE / RAZOR: the samples and the razor slice editor

SAVE opens FILE page 1, **SAMPLE**: SELECT moves the cursor, SAVE loads the file under it into the track's slot. When
the cursor is on the track's own file, SAVE opens page 2, **RAZOR**, for that file; SAVE there goes back. The razor
machines (RAZOR, R.POLY) play each slice from its own start / end. The table is per track and saved with the project;
its per-slice settings (PLAY, CHOK, P/T, mute) apply to every slice machine.

| On RAZOR | Does |
|---|---|
| Lower keys 1-16 | Select that slice and play it (velocity); slices 17-32 when the selected slice is in that bank |
| FN + lower key | Mute / unmute the slice in the razor table (it stays on its steps, plays nothing) |
| SELECT | Select the slice |
| KN1 / KN2 | **STRT / END** of the selected slice (0..1 of the track window), under the SNAP mode |
| KN3 | **PLAY**: the slice's play mode (TRACK = the track's) |
| KN4 | **ZOOM** 1X..64X, centred on the point you last moved |
| FN + KN1 | **SNAP**: GRID (next slice_count division), ZOOM (0.5 / zoom per detent, the plain mode), ZERO-X (the nearest zero crossing), TRANS (the next / previous transient), FRIEND (also drags the touching neighbour point) |
| FN + KN2 | **CHOK**: the slice's choke group A-H (a new voice in the group cuts the others; OFF) |
| FN + KN3 | **P/T**: the slice's pitch offset |
| FN + KN4 | **SENS**: Auto-Chop / transient sensitivity (the ticks over the waveform) |
| OCT+ | **Split** the selected slice at the point you last moved (else in the middle) |
| OCT- | **Delete** the selected slice |
| FN + OCT+ | **Auto-Chop**: one slice per transient (the head before the first one left out), the slice count follows |
| FN + OCT- | **Redistribute**: the slices in use as even divisions |

The OCT keys mean this only while RAZOR shows: there a held OCT opens no PAGE overlay and a tap changes no key mode.
Split and delete renumber the slices after it; the track's steps follow (a step on slice 6 stays on that audio).
The screen: the waveform of the track window (trim x Range), the selected slice as a cream block, slice numbers,
the transients at SENS as ticks, the sounding voices in orange, the zoom window as a bar, the 16 pads of the bank.

## FX and sends

- **18 FX machines** (NONE, DELAY, REVERB, LOFI, COMP, DESTROY, ECHO, BLACKHOLE, CHORUS, FLANGER, PHASER, DJ EQ, DUCK,
  TAPE ECHO, CASSETTE, MOTION, RINGS, LIMIT) on four kinds of slot: each track's insert, the S1 and S2 send returns,
  the master insert. The machine is chosen in the setup (**FN + FX**, KN1) so a knob turn never swaps it by
  accident. On a send, NONE returns nothing.
- The FX page shows the machine's cells (FN for cells 5-8) over a picture: the delay taps, the reverb's decay, the
  compressor / limiter curve with the live IN and GR meters, the crushed wave, the sweep, the EQ bands; the bar at
  the right is the slot's output level.
- **SENDS** (FX, page 2): S1 / S2 levels and TAP (PRE = after the filter, POST = after the insert). A muted track
  still feeds its sends (as the original). The MIX page meters the S1 / S2 returns.
- **DUCK** listens to the dry program: on a send return, the six tracks; on an insert, the tracks before it; on the
  master, the master.
- A stopped track that has gone quiet for 4 s stops using CPU until it plays again (the original's idle saver).

## MOD

Two LFOs (DEST, WAVE, SPD synced, DEP bipolar, MODE free / trig / one / hold), the mod envelope (ADSR, DEST, DEPTH;
retriggered where the step's ERST is on, the LFOs where LRST is on), velocity (slice trigs) and four macros. DEST:
PITCH (+-12 st), CUTOFF, RES, AMP, PAN, or a macro (M1-M4). PITCH reaches every warp mode as in the original: the
readers, the wavetable oscillator, the spectral pitch shift and each harmonizer interval. The bar at the right of the
LFO / env / velocity pages is the live value at the destination. Depths, speeds, the env times and macro values
p-lock; DEST, WAVE and MODE do not (as the original).

## Scenes and the crossfader

Two scenes, A and B, each a sparse set of param values for every track and the three buses; the fader morphs between
them. **MST > SCENE** (HOME, page 3) is the overview: the fader with its six glide positions and where a glide is
heading, and the params that morph (track or bus, label, both ends, where the morph is now); **KN1 = XFD**, the fader
as a direct 0..128 knob. The scene row is on the lower keys in the SCENE key mode (a strip with the fader sits over
the page) and always on MST > SCENE: lower keys 9-16 = A, six glide keys, B (the grid's row 5), mirrored on 1-8.

| Gesture (scene row) | Does |
|---|---|
| FN + A (key 1 / 9), FN + B (key 8 / 16) | Capture every morphable param of every track and bus into the scene (sparse: a param at its default is stored only when the other scene holds it) |
| Hold A or B + turn a cell | That param (selected track) into the held scene only; the sound does not move until the fader does. A cell shows the scene's value in orange while the anchor is held |
| Hold A or B + PLAY | Clear that scene |
| Hold a glide key (2-7 / 10-15 = 0, 0.2 .. 1) | Ease the fader there (5 /s exponential, lands in ~1 s); release freezes; another key still held takes over |
| FN + hold a glide key | The slow, linear move: the whole A..B travel in 5 s |
| MST > SCENE KN1 (XFD) | The fader, 0..128 |

Morphing is the base-value resolver's source 2: a firing step's p-lock wins, then the fader, then the track value.
The track value is never written; turning a param's knob by hand gives it back to the knob until the fader next
moves. A param one scene does not hold morphs against its track value. Only continuous params morph: every track's
(warp, FX, MOD params included), and the buses' FX params; not options, lengths, slots, slice counts, rates or the
track volume. The scenes and the fader are saved with the project (one pair for all patterns) and re-applied after a
pattern switch.

## Copy / clear / paste (each scope keeps its own buffer)

| Scope held | REC = copy | PLAY = clear | FN + PLAY = paste |
|---|---|---|---|
| Step(s) | the held steps with their locks | their p-locks (the trig stays) | onto the held step(s), the copied steps' spacing kept (past the page edge: dropped) |
| Slice pad(s) (loop layer, slice machines) | the pad's program: locks, choke, mute | the programs | onto the held pads |
| PAGE (an OCT key held) | the step page | the step page | onto the step page |
| PATN (held) | the pattern: params, steps, BPM, page loops, slice programs | **the trigs** of every track, after a second PLAY (the box says CLEAR P01? PLAY) | onto the current pattern (its name stays) |

A toast in the middle of the screen says what happened.

## PAGE (hold OCT- or OCT+, not on RAZOR)

While an OCT key is held the screen and the lower keys show the pattern's pages. **SELECT** picks the mode:

| Mode | A key | The LEDs |
|---|---|---|
| SEL | the step page the keys edit | the selected page bright, the playing page blinks |
| LOOP | the page in / out of the playing loop (never empty) | pages in the loop mid |
| RATE | the track's pattern rate (16 of the 21: key 9 = 1x) | the current rate |

## Memorize / recall, undo, names

| Gesture | Does |
|---|---|
| FN + OCT+ | **Memorize**: the whole project (patterns, params, pool, scenes) to RAM |
| FN + OCT- | **Recall** it, also while playing (the pattern restarts at step 1; only changed samples reload). OCT- lights with FN once something is memorized |
| FN + SAVE | **Undo** the last change, 16 deep, per gesture (a knob turn, a fader glide, a step tap, a lock, a paste or a page clear); a pause of a second starts a new gesture |
| PATN overlay open + SAVE | **Name the pattern** |
| MST setup NAME (turn) | **Name the project** |
| Naming screen | KN1 / SELECT the place, KN2 the character, KN3 a random name, KN4 left = delete, right = insert a space; **SAVE** keeps it, a tap of FN cancels |

## Slices and patterns

- Hold slice pad(s) (loop layer) + turn a lockable cell = that slice's own lock in the pattern (its pitch / velocity
  per voice, the rest held for the note under the step's own locks). The slice SOURCE page has **CHOK** (choke group
  NONE, 1-8) and **SMUT** (mute) for the held pads, else for the SLIC slice.
- A slice has two homes: the **razor table** (per track, every pattern: range, PLAY, CHOK, P/T, mute) and the
  **pattern's slice program** (locks, CHOK, SMUT). A slice is silent when either mutes it; the pattern's choke group
  wins, else the razor table's.
- BPM is **per pattern**: a pattern switch brings its BPM. MST setup **GBPM ON** = one BPM for all patterns.

## LEDs

Steps: trig = mid, p-locked trig = flicker, ghost = dim, playhead = bright. Loop keys: the region dim, the playhead's
key mid, held keys bright. RAZOR: the bank's slices dim, the selected one bright, the sounding ones mid, muted ones
faint, both OCT keys dim. Upper keys: the
selected track bright, the others by their level, muted off. Category buttons: the current one lit (blinking in
setup). PLAY green while playing. REC dim when the project has unsaved changes.

## Differences from the norns version

- **The panel.** The grid's rows (loop keys, steps, keyboard, pattern slots, the crossfader) share the 16 lower keys:
  SEL held, the OCT key modes, the PATN overlay and PAGE switch them. STOP is FN + PLAY; the grid's PAGE key is an OCT
  key held; the norns K2/K3 pair navigation is four knobs on four cells (FN for the next four). Undo (the grid's NO)
  is FN + SAVE, memorize / recall FN + OCT.
- **Razor.** The table is per track (the original shared one global table); the plain STRT / END turn is the ZOOM
  snap and the other snaps act on every turn; split / delete / per-slice play mode + pitch are added; the onset
  envelope reads 32 frames per window (the Lua 8).
- **FX.** The bus FX (S1, S2, MST) are global and not p-lockable (the original's bus lanes are not ported); they do
  morph with the scenes, as in the original. The macro depth matrix is edited on the MACRO 1-4 pages (the original
  used a grid gesture). The FX machine list is the original's 18 (DRIVE removed): a 0.1.0 project's stored insert
  machine lands on a different machine (none was audible in 0.1.0).
- **Patterns.** The pattern scope of copy / clear / paste is PATN held (the grid's FN + REC / PLAY / STOP were removed
  there; FN + REC saves here); the STOP-held CUT/PASTE mode is not ported. Page loop plays (the original stores it but
  ignores it in playback); RATE in the PAGE overlay sets the track's pattern rate. Per-page rates are not done.
- **Scenes.** The scene base is the track value itself (the original's stashed base always equals it). Hold an anchor
  + PLAY clears the whole scene (the original clears one param of both scenes with K2/K3).
- **Slices.** Slice mute is the SMUT cell (FN + key = 17-32 here) or FN + key on RAZOR. Slice locks apply to live pads
  for pitch / velocity only.
- **Not ported.** Live step trig (an editor pref), the step preview pref, the visualizer, audio input (E6). Names use
  A-Z 0-9 and a few signs (the 3x5 font has no lower case). Undo covers params, globals, the fader, step edits,
  pastes and page clears; not scene edits, pattern-scope operations, sample loads or recalls (as the original).
- The engine's own deviations (control rate, feedback loops, click-free swaps, ...) are listed in src/dsp/README.md.
