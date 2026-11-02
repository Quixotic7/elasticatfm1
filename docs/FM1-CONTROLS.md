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
their p-locks (the trig stays). Copy / paste, scenes, PAGE, memorize, undo and names: see **E5** below.

## Buttons

| Button | Category | Tap again | FN + tap |
|---|---|---|---|
| SEQ | TRIG: NOTE LEN VEL, resets / COND: chance, condition, ratchet / MTRIG or SLICE | next page | swing |
| SEL | SRC: SOURCE / MACHINE / WARP / RANGE (acts on release) | next page | setup: machine, warp, loop division, poly |
| EDIT | FILT: FILTER (curve) / F.ENV / F.MIX | next page | setup: filter machine, filter env mode, mix mode |
| ENV | AMP: envelope, VOL, PAN, sends | - | setup: env mode, env range, portamento, slice hold |
| FX | FX, SENDS (stored; heard from E4) | next page | - |
| LFO | MOD: LFO 1, LFO 2 (stored; heard from E4) | next page | - |
| GLO | MASTER: MIX (meters + faders) / MASTER (BPM, swing, global length, playhead return) / SCENE (E5) | next page | setup (+ GBPM, NAME) |
| SAVE | FILE: the sample browser | **loads the file under the cursor into the track's slot** | - |
| ARP | PAT: length, rate, global length, change mode | - | setup |
| PLAY | Play / pause (resume keeps the position); with a scope held = CLEAR (E5) | | **stop + back to step 1** (twice: all voices off); with a scope held = PASTE |
| REC | with a scope held = COPY (E5) | | **save the project now** |
| OCT- / OCT+ | Tap: lower keys STEPS / LOOP KEYS / KEYS / SCENE (OCT- lit = loop keys, OCT+ lit = keyboard, both = scene). **Hold = PAGE** (E5) | | OCT+ **memorize**, OCT- **recall** (E5) |
| SAVE | (FILE, above) | | **undo** (E5) |

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
- STOP is FN + PLAY; the grid's PAGE key is an OCT key held (E5).
- The norns K2/K3 pair navigation is replaced by four knobs on four cells (FN for the next four).
- Macros and the visualizer: not yet. E5's differences are listed at the end of its section.

## E5: scenes, copy / clear / paste, PAGE, memorize, undo, names

### The mapping, and why

The grid gives Elasticat a row for each of these; the FM-1 has 16 lower keys, 11 upper keys (all taken: T1-T6, S1,
S2, MST, FILL, PATN) and four knobs. So E5 reuses what is already learned: **REC / PLAY / FN+PLAY are COPY / CLEAR /
PASTE** exactly as on the grid (STOP is FN + PLAY here), with the same scopes held; the lower keys get one more mode
(SCENE) and one more held layer (PAGE); FN + OCT and FN + SAVE take the grid's FN + OCT and NO.

- **Scenes live on the lower keys, as a fourth key mode (SCENE).** The grid's crossfader row (row 5, columns 9-16:
  A, six glide keys, B) lands 1:1 on lower keys 9-16, and is mirrored on keys 1-8 so either hand can play it (one
  octave = C for A, D..B the glide, the top C for B). The glide keys must be *held* (the ease runs while held,
  release freezes, FN makes it a slow 5 s move) - that wants real keys, not a knob. Being a latched key mode (OCT- once
  from STEPS; both OCT LEDs lit), it leaves the knobs on whatever page is showing, so **hold A + turn any cell**
  writes that page's param into scene A only, like the grid's hold-anchor + E2/E3. A strip with the A/B fader sits over
  the top of the page while the mode is on.
- **MST > SCENE** (GLO, page 3) is the overview: the fader with its six glide positions and where a glide is heading,
  and the params that morph (track, label, both ends, where the morph is now). **KN1 = XFD**, the fader as a direct
  0..128 knob for slow manual sweeps. On this page the lower keys are the scene row whatever the key mode.
- Rejected: FN + upper keys (no free upper keys, and FN + T is mute); a crossfader only on its page (scene locks of
  other pages' params would be impossible).

### Scenes (lib/scene_store.lua)

| Gesture (scene row) | Does |
|---|---|
| FN + A (key 1 / 9), FN + B (key 8 / 16) | Capture every morphable param of every track into the scene (sparse: a param at its default is stored only when the other scene holds it) |
| Hold A or B + turn a cell | That param (selected track) into the held scene only; the sound does not move until the fader does. A cell shows the scene's value in orange while the anchor is held |
| Hold A or B + PLAY | Clear that scene |
| Hold a glide key (2-7 / 10-15 = 0, 0.2 .. 1) | Ease the fader there (5 /s exponential, lands in ~1 s; ~80 % after 0.3 s); release freezes; another key still held takes over |
| FN + hold a glide key | The slow, linear move: the whole A..B travel in 5 s |
| MST > SCENE KN1 (XFD) | The fader, 0..128 |

Morphing is the base-value resolver's source 2: a firing step's p-lock wins, then the fader, then the track value.
The track value is never written; turning a param's knob by hand gives it back to the knob until the fader next
moves (BASE_VALUE_RESOLVER.md). A param one scene does not hold morphs against its track value (the original's
"base"); only continuous params morph (not options, not lengths / slots / slice counts / rates / the track volume).
The scenes and the fader position are saved with the project (one pair for all patterns, as the original) and
re-applied after a pattern switch.

### Copy / clear / paste (Elektron-style; each scope keeps its own buffer)

| Scope held | REC = copy | PLAY = clear | FN + PLAY = paste |
|---|---|---|---|
| Step(s) | the held steps with their locks | their p-locks (the trig stays) | onto the held step(s), the copied steps' spacing kept (past the page edge: dropped) |
| Slice pad(s) (loop layer, slice machines) | the pad's program: locks, choke, mute | the programs | onto the held pads |
| PAGE (an OCT key held) | the step page | the step page | onto the step page |
| PATN (held) | the pattern: params, steps, BPM, page loops, slice programs | **the trigs** of every track, after a second PLAY (the box says CLEAR P01? PLAY) | onto the current pattern (its name stays) |

A toast in the middle of the screen says what happened.

### PAGE (hold OCT- or OCT+)

While an OCT key is held the screen and the lower keys show the pattern's pages (a tap of OCT still changes the key
mode). **SELECT** picks the mode (tabs at the top):

| Mode | A key | The LEDs |
|---|---|---|
| SEL | the step page the keys edit | the selected page bright, the playing page blinks |
| LOOP | the page in / out of the playing loop (never empty) | pages in the loop mid |
| RATE | the track's pattern rate (16 of the 21: key 9 = 1x) | the current rate |

### Memorize / recall, undo, names

| Gesture | Does |
|---|---|
| FN + OCT+ | **Memorize**: the whole project (patterns, params, pool, scenes) to RAM |
| FN + OCT- | **Recall** it, also while playing (the pattern restarts at step 1, like a direct start; only changed samples reload). OCT- lights with FN once something is memorized |
| FN + SAVE | **Undo** the last change, 16 deep, per gesture (a knob turn, a fader glide, a step tap, a lock, a paste or a page clear); a pause of a second starts a new gesture |
| PATN overlay open + SAVE | **Name the pattern** |
| GLO setup NAME (turn) | **Name the project** |
| Naming screen | KN1 / SELECT the place, KN2 the character, KN3 a random name (namesizer), KN4 left = delete, right = insert a space; **SAVE** keeps it, a tap of FN cancels |

### Slices, per-pattern BPM, loop keys 17-32, step preview

- Hold slice pad(s) (loop layer) + turn a lockable cell = that slice's own lock (its pitch / velocity per voice, the
  rest held for the note under the step's own locks). The slice SOURCE page has **CHOK** (choke group NONE, 1-8) and
  **SMUT** (mute) for the held pads, else for the SLIC slice. A muted slice is silent on steps and pads (its LED faint).
- **FN + a loop key / pad = 17-32**, when the track has them (LDIV above 16, more than 16 slices); the LEDs show
  17-32 while FN is held.
- BPM is **per pattern** (the original's default): a pattern switch brings its BPM. GLO setup **GBPM ON** = one BPM
  for all patterns.
- Holding a trig step while stopped plays it (step preview): slices as pads, loops through the loop keys' span.

### Where E5 differs from the original

- Copy / clear / paste of the pattern is on PATN held (the grid had FN + REC / PLAY / STOP, later removed there as
  too easy to hit by accident; FN + REC is save here). The STOP-held CUT/PASTE mode is not ported (FN + PLAY is stop).
- Page loop *plays* (the original stores the page loop but its playback ignores it); RATE in the PAGE overlay sets
  the track's pattern rate (the original lists the mode but leaves it unwired). Per-page rates would break the
  derived clock and are not done.
- Scene bases are the track value itself (the original's stashed base follows every edit, pattern load and undo, so
  it always equals the track value). Hold an anchor + PLAY clears the whole scene (the original clears one param of
  both scenes with K2/K3).
- Slice mute is the SMUT cell, not FN + slice key (FN + key = 17-32 here). Slice locks apply to live pads for pitch /
  velocity only (the original also previews the track-level ones while stopped).
- Live step trig (an editor pref, off by default in the original) is not ported. Step preview has no on/off pref.
- Undo covers params, globals, the fader, step edits, pastes and page clears; not scene edits, pattern-scope
  operations, sample loads or recalls (as the original).
- Names: 16 characters from A-Z 0-9 and a few signs (the 3x5 font has no lower case).
