# Elasticat FM-1 0.6.0: modes and key shortcuts (spec, 2026-11-03)

The owner proposed this on 2026-11-03 and settled it in six rounds of questions, with one mockup per mode.
**Status: built in 0.6.0** (2026-11-03); what the build decided where this spec is silent or could not be followed is
in §8. The panel as built: docs/FM1-CONTROLS.md.

It replaces 0.3.0's category / lane model (docs/UX-REDESIGN.md) and 0.5.0's MOD KEYS mode (docs/MOD-MATRIX.md §3).
The 0.5.0 matrix itself stays.

## 0. Words used here

- **Black keys B1..B11** (F#3..F#5) and **white keys W1..W16** (F3..G5). These are the same names FIZZ uses.
- Only keys F3..C5 have pressure (RemoteFM1 PLAN.md: keys 0..19). That is **W1..W12 and B1..B8**.
- **F1 = the OCT- button, F2 = the OCT+ button.**
- **SEL** = the SELECT knob. **ALGO** = the ALGORITHM knob. **PRESETS** = the PRESETS knob. **K1..K4** = KNOB 1..4.
- Buttons are called by their printed names: FX SEL ENV LFO EDIT GLO HOME FILE (printed SAVE) ARP SEQ PLAY REC.
- A **mode** is what a button opens. Each mode has **pages**.
  - **Tapping the lit mode button again** goes to the next page, or for HOME and EDIT to the next sub-mode.
  - **Each mode remembers its page** when you leave it and come back.
- **"Settings"** means the mode's setup pages. You open them with **a hold (400 ms) or SHIFT + the button**, and they stay open after you let go.

## 1. Global rules (every mode, unless the mode says otherwise)

| Control | Does |
|---|---|
| White keys | **select steps**. Hold a step + turn a knob = lock that param on the step. Hold a step + turn **PRESETS** = lock a sample slot on the step |
| Black keys | per mode (§3) |
| ALGO | the step page (1-16, 17-32 ...) |
| PRESETS | the track's sample slot (the pool, §5) |
| SEL | the page / row (as today) |
| K1..K4 | the page's four values. SHIFT + a value knob snaps it (as today) |
| F1 | tap = sticky on/off, hold = on while held. While on: **turn a knob = clear every step lock of that param** (selected track); **white keys = copy / paste steps** (§2) |
| F2 | the same, but **turn a knob = clear every modulation of that param** (matrix slots aimed at it); **white keys = cut / paste steps** |
| F1 + F2 | F3 (pattern length, rate, launch quantize; also **hold SEQ / SHIFT + SEQ**) |
| GLO = SHIFT | hold = held. A quick tap = **sticky SHIFT**, which stays on until GLO is tapped again or the mode changes |
| PLAY | **start / stop at once, on press** |
| REC | live record on / off, in any mode (§4). Hold / SHIFT + REC = record settings |

**SHIFT layer:**

| SHIFT + | Does |
|---|---|
| SEL | BPM |
| ALGO | swing |
| PRESETS | the track (prev / next) |
| White keys W1..W9 | select the track: T1..T6, S1, S2, MST |
| F1 / F2 | UNDO / REDO |
| FILE (SAVE) | save the project |
| Black keys | shortcuts: B1 TAP TEMPO, B2 LATCH, B3 MEMORIZE, B4 RECALL, B11 FILL. B5-B10 are free (labels dark) |
| A mode button | that mode's settings |

The SHIFT keys light while SHIFT is held, and their labels come from `ec_app_labels`.

**The source template** applies in FILTER, AMP, MODULATION, LOOP ENGINE and FX:

| B1 | B2 | B3 | B4 | B5 | B6 | B7 | B8 | B9 | B10 | B11 |
|---|---|---|---|---|---|---|---|---|---|---|
| page 1 (hold = quick setting) | page 2 | page 3 | page 4 | page 5 | LFO 1 | LFO 2 | MOD ENV | VEL | AT | the mode's special key |

- **B6-B10, the sources:** hold one + turn any value knob = route that source to that param and set the depth. This is 0.5.0's MOD KEYS gesture, now always present.
  - Add F2 while turning = remove that route.
  - Hold steps as well = depth locks.
  - A tap opens the source's page in MODULATION.

## 2. Step copy / cut / paste (F1 / F2 on)

There is a one-step **buffer**.

- **Copy (F1).** The first tap on a step copies it into the buffer. The next tap pastes the buffer there, onto any step, even one that isn't empty.
  - After that, a tap on **a step that isn't empty copies it** (a new buffer), and a tap on **an empty step pastes**.
- **Cut (F2).** It is the same, but taking a step into the buffer empties it.
  - So cutting a step, then tapping another step, **moves** it.
  - Cutting a step and tapping it again puts it back, and further taps on empty steps paste copies.
- The buffer empties when F1/F2 turns off.
- In PAGES, F1 / F2 do the same with **whole pages** on the black keys. In PATTERN LAUNCH they do it with **patterns** on the white keys.

## 3. The modes

| Button | Mode | Tap again | Settings (hold / SHIFT +) |
|---|---|---|---|
| FX | FX | next FX page | FX settings |
| SEL | FILTER: FILTER / FILTER ENV / MIX pages | next page | filter settings |
| ENV | AMP: AMP / AMP ENV / MOD ENV pages | next page | amp settings |
| LFO | MODULATION: MATRIX / LFO 1 / LFO 2 / VEL / MACROS | next page | (none) |
| EDIT | **MIX ↔ PAGES** | the other one | track settings |
| HOME | **LOOP ENGINE → TRIG → KEYBOARD** | the next one | the machine / loop settings |
| FILE (SAVE) | FILE (browser, editor, razor) ↔ SAMPLER | the other one | file settings |
| ARP | CROSSFADER | | scene settings |
| SEQ | PATTERN LAUNCH | | F3 |

**APP SETTINGS ("everything else"): SHIFT + HOME** (owner, 2026-11-03). This is the one exception to "SHIFT + a button = that mode's settings"; holding HOME still opens the loop / machine settings.
- The rows:
  - **THEME**: several 16-colour themes. Each is just a palette table. The theme changes as you scroll, so you see it before you leave.
  - the mode pop-up on/off
  - knob speed
  - auto-preview in FILE
  - aftertouch on/off
  - the metronome sound
  - MIDI CC 1 / 11 inputs on/off
  - anything else that isn't per pattern or per sound
- **Saved globally**, not per project: they go in their own record in the cart's flash image, so they survive project loads and new projects.
- Pop-up rules apply: F1 = back, F2 = OK, and HOME closes.
- Themes to start with (named in the theme picker):
  - **CREAM**: today's look, the default
  - **AMBER**: amber monochrome CRT
  - **PHOSPHOR**: green on black
  - **DMG**: four-green handheld
  - **ICE**: cyan on navy
  - **PAPER**: dark on cream, inverted

**Entering a mode** (from another mode, or HOME / EDIT / FILE stepping to their next sub-mode) shows a **pop-up** for about 0.8 s. It has:
- the mode's name, big
- its current page
- a strip of the 11 black keys in their roles (page / source / special / unused)

It goes away on its own, or on any knob or key, and that knob or key still does its job. Turning to the next page inside a mode shows no pop-up.

**Power-on / project load:** LOOP ENGINE. The old HOME "back to the main view" is gone: every mode is a main view.

### FX
- **Black keys:** B1 INSERT 1, B2 INSERT 2, B3 SEND 1, B4 SEND 2, B5 MASTER. B6-B10 are the sources. **B11 = MIX** (the FX mix page).
- **Hold an FX slot key (B1-B5)**, then change that slot's FX engine with the white keys (one engine per key), K1, or SEL.

### FILTER (SEL)
- Pages: FILTER, FILTER ENV, MIX (from the layout).
- Black keys follow the template. **Hold B1 = filter type**, changed with K1 or SEL.
- B11 = filter env on/off (or the env mode).

### AMP (ENV)
- Pages: AMP, AMP ENV, MOD ENV.
- Black keys follow the template. Hold B1 = the env mode.

### MODULATION (LFO)
- **B1 = MATRIX.**
- **B2-B5 = M1-M4.** A tap opens that macro's page. Hold + turn routes that macro, like the other sources.
- B6-B10 are the sources (a tap opens their page). **B11 = MACROS overview.**
- **The macro values can be sequenced:** they are lockable and recorded as automation.

### MIX (EDIT, first)
- **White keys W1..W9 = the tracks** (T1-T6, S1, S2, MST).
  - A tap selects the track. **A hold toggles mute.** SHIFT + a track key toggles solo.
  - FILL held + a track key clears every mute and solo.
- **B11 = the CAT** (owner, 2026-11-03). The norns Visualizer page: `images/dancingcat.png`, 12 frames, looping, at a speed that follows the tempo (120 BPM = 1x). It fills the screen, converted to the palette on the 120x120 grid. B11 or any mode button leaves it, and the white track keys keep working while it shows.
- **Black keys B1..B10 = the selected track's rate:** B1..B9 for the rate list, B10 / B11 for slower / faster.

### PAGES (EDIT, second)
- **Black keys = the pattern's step pages**, FIZZ style.
  - A tap edits the page. A hold (0.4 s) puts the page in or out of the loop.
- With F1 or F2 on, they copy / cut and paste pages (§2).

### LOOP ENGINE (HOME, first)
- Black keys follow the template: B1..B5 are LOOP, MACHINE, WARP, RANGE and the next page from the layout.
- **B11 cycles the zoom:** 16 → 32 (first half) → 32 (second half).
- **Live grains with the GRAIN warp** (owner, 2026-11-03). Whenever the waveform is shown (LOOP ENGINE's pages, and the WARP page above all), each active grain is drawn over the waveform:
  - a block at its read position, as wide as the grain's length
  - its envelope level shown in steps of the grey ramp, plus the accent for the newest grain
  - spray and pitch visible as the scatter
  - The engine publishes a small grain snapshot once per screen frame: position, length, level, at most about 16 grains per track.
  - It follows the 120x120 / 16-colour rules.
  - The other warps keep the playhead.
- **White keys play the loop** (0.4's loop keys). On the RANGE page they play the range.

### TRIG (HOME, second)
- Pages: TRIG, COND, PLAYHEAD. Black keys follow the template.

### KEYBOARD (HOME, third)
- **The keybed is a full keyboard** for the selected track.
- **F1 = octave down, F2 = octave up.** That replaces copy / cut in this mode.
- **On slice tracks (SLICE / RAZOR):** the **black keys pick the slice** (10 at a time). **B11 steps the slice bank** (1-10, 11-20 ...). The white keys play the selected slice at a pitch.
- **ALGO = the target step** for step entry. Turning it below step 1 means "live" (no entry). With a target step, playing a key writes a trig with that note or slice.
  - **Advance or stay** is a KEYBOARD setting (HOME settings), ADVANCE on/off, defaulting to on. Either way SHIFT + the key does the other.
  - Chords are not stacked: one note per step.

### FILE (FILE, first): browser, editor, razor
- **It can't open while playing.** The button shakes or flashes "STOP FIRST". Inside FILE, **PLAY and REC do nothing**. Leave FILE to play.
- **Browser:**
  - SEL scrolls the files, **walking folders and subfolders**. **B1 = up a folder, B2 = into the folder** under the cursor.
  - Every move **previews** the file (auto-preview on by default, a setting).
  - **B11 = preview** (play / stop).
  - **B10 = LOAD** puts the file in **the next free pool slot**, and the cursor stays put. Load, scroll, load ... so 50 samples are a minute's work.
- **The pool meter:** a bar shows the arena used / free and the slots used of 128.
- **Editor:**
  - Black keys = the zoom levels. **B1 = scroll left, B11 = scroll right** when zoomed.
  - **White keys set the trim:** the first tap sets start, the second sets end.
- **Razor:** as 0.4.
- **F2 on a pool slot frees it. SHIFT + B11 = purge** (frees every slot no pattern uses). Loading into a full pool shows FULL.

### SAMPLER (FILE, second): not gated
- **B11 arms.** Recording starts on the next bar, or at once when stopped. B11 again stops at the end of the bar.
- The length is a setting: free / 1 / 2 / 4 / 8 bars.
- The take is saved to /data/rec and **lands in the next free pool slot**, tagged with the BPM.
- **White keys = the input level meter**, lit like a VU.
- The source is 0.4's ain1 input.

### CROSSFADER (ARP)
- **B1 = scene A, B2 = scene B.**
  - **Hold one + turn a knob** = lock that param to the scene (the selected track, like a step lock).
  - **B11 held + A / B = capture:** every param of every track goes into that scene.
  - **SHIFT + A / B = clear** that scene's locks.
- **W1..W12 = the crossfader**, W1 = 0 and W12 = 127.
  - **Hold a key** and the fader glides to that key's value.
  - **More pressure = faster glide.** Let go and the fader stays where it is.
- **SHIFT + W1..W12 = jump** there at once.
- **W13-W16 set the base glide time:** instant / fast / medium / slow. That time is used with no pressure (the on-screen panel), and pressure scales it.
- **The crossfader is never sequenced or recorded.** It is a live control only.
- Memorize / recall are SHIFT + B3 / B4 everywhere.

### PATTERN LAUNCH (SEQ)
- **4 banks × 16 patterns = 64** (EC_NPAT 16 → 64).
- **White keys launch** the bank's patterns, with the launch quantize from F3.
- **B1-B4 pick the bank.**
- F1 / F2 copy / cut and paste patterns.
- Hold SEQ, or SHIFT + SEQ, = F3, which also has the pattern name.

## 4. Live recording (REC)

- **REC toggles recording in any mode except FILE.** The REC LED is lit while recording.
- **What gets written:**
  - **notes from KEYBOARD**, with pitch, velocity and slice
  - **LOOP ENGINE key presses**, as a trig plus the loop position locked
  - **knob moves**, as locks, on ghost trigs where there's no trig; this includes **the macro values**
  - **Not the crossfader.**
- **Timing:** each hit goes to the nearest step and keeps its early or late offset as a micro-timing lock. A setting turns the offset off (hard quantize).
- **Record settings** (hold REC / SHIFT + REC):

  | Setting | Choices |
  |---|---|
  | MODE | **CAPTURE**, **FIXED**, **OVERDUB** |
  | LENGTH | for FIXED: 1 / 2 / 4 / 8 bars |
  | COUNT-IN | 0 / 1 / 2 bars |
  | METRONOME | off / rec only / always, with a level |
  | AUTOMATION | on / off |
  | QUANTIZE | offset kept / hard |

  - **CAPTURE** records as played. If playback is stopped, it starts on the first key or loop-key press.
  - **FIXED** records for a fixed length and then stops recording; playback continues.
  - **OVERDUB** adds to and replaces steps without clearing.
  - **AUTOMATION** records the knob moves (ghost locks).
- **The metronome and count-in click** play on the main output, **after the master bus**, so they are never resampled. The click has its own level.

## 5. The sample pool

- **128 slots** (EC_NSLOT 16 → 128). The project file format version goes up, and older projects load into slots 1-16.
- **No per-sample length limit.** The arena takes whatever is free.
- **Memory:** `MEMORY_MIB 256` (from 128), and the arena grows to about 224 MiB, roughly 22 minutes of stereo at 44.1 kHz.
  - The owner wants the use visible: FILE's pool meter, and a small bar in PRESETS' takeover.
  - Hosts refuse a cart that doesn't fit ("needs N MB"), per VIRTUAL-CARTS.md §2.
- **PRESETS** walks the used slots, skipping empty ones, and you hear the change at once.

## 6. What goes away

| Gone | Replaced by |
|---|---|
| The lower-key modes from the OCT keys (STEPS / LOOP KEYS / KEYS / SCENE) | the modes LOOP ENGINE / KEYBOARD / CROSSFADER; steps are the default |
| SRC held = loop keys | LOOP ENGINE |
| MOD KEYS mode (hold LFO) | the source template on the black keys |
| The track keys on the black row | SHIFT + W1..W9, and MIX |
| The MST key | MIX's MST, and SHIFT + W9 |
| PATN | PATTERN LAUNCH |
| FILL key | SHIFT + B11 |
| SHIFT + OCT = undo / redo | SHIFT + F1 / F2 (same meaning, new names) |
| OCT+ held + knob = clear lock | F1 + knob |
| Pop-ups' OCT- = back / OCT+ = OK | stays (F1 = back, F2 = OK) inside pop-ups only |

## 7. Must hold for the build (after the owner's OK)

- **Every screen is designed at 120x120 and shown at exactly 2x** (owner, 2026-11-03), so every pixel is a chunky 2x2 block.
  - **No 1-pixel details anywhere:** no hairlines, ticks, borders, dots or gaps finer than one 120-grid pixel, and no anti-aliasing.
  - **16 colours** (owner, 2026-11-03):
    - Screens draw 4-bit palette indices into a 120x120 buffer.
    - One 16-entry table turns the indices into RGB565 at 2x, so a theme is just a different table.
    - Every colour has a named slot (ground, the grey ramp, accent, role colours). Nothing is drawn outside the palette, and a test checks it.
  - **A step-locked cell fills its whole box** with the LOCK colour: label, K-tag, value and bar are all drawn dark on it (owner, 2026-11-03). Before, only the value was highlighted.
  - **Filter and envelope pictures are the norns script's bars** (owner, 2026-11-03), from Elasticat `lib/ui/page_render.lua`: `draw_filter_bars`, `env_bars` / `draw_envelope_bars`. They keep the same stage proportions, a 0-length stage drawn as a cliff, and the selected stage highlighted. This applies to FILTER, FILTER ENV, AMP ENV and MOD ENV.
  - **The intro is the norns logo sprite**, `images/q7logoanim.png`: 55 frames, 10 fps, about 5.5 s at power-on. It is converted to the palette on the 120x120 grid, and any key or knob skips it.
  - This applies to the existing screens too.
  - A test checks that every 2x2 block of the LCD is one colour.

- The 0.5.0 tests are rewritten for the new gestures, and every gesture in this file gets a harness test.
- The layout tool knows the modes: the categories become the modes' page lists.
- Worst case ≥ 2x real time in wasm3, measured at 256 MiB.
- 0.6.0.

## 8. Built in 0.6.0

Where the spec was silent, or a point could not be built as written, the build chose the simplest thing consistent
with the spec, FIZZ and ChoralRoot:

**Modes and keys**
- FX (0.6.1 adds a second insert, section 9): there is one insert per track, so **B2 is the track's SENDS page**;
  B3 / B4 / B5 select the S1 / S2 / MST bus pages, B11 the FX MIX page (FX MIX, DRIVE, SEND 1, SEND 2).
- AMP gained an AMP page (LEVEL, PAN, VELOCITY, VEL DEPTH / SEND 1, SEND 2); AMP ENV keeps the envelope; MOD ENV
  moved here from MODULATION. AMP and TRIG have no B11; LOOP ENGINE has four pages (no B5).
- Hold B1 = the quick setting: FILTER the filter type, AMP the env mode, LOOP ENGINE the machine, FX the insert's
  engine. FILTER's B11 toggles the filter env mode ADSR / AHR.
- The source keys' taps: LFO 1 / LFO 2 / VEL open MODULATION's page, MOD ENV opens AMP's MOD ENV page, AT the MATRIX.
- LOOP ENGINE's zoom (B11) is the track's LOOP DIV param (16, 32 first half, 32 second half): saved, undoable.
- MIX: B1-B8 are the eight rates 1/8 .. 4X (4X appended to the rate table), B9 slower, B10 faster, B11 the CAT.
  Holding S1 / S2 / MST selects it (buses have no mute). The rate keys do nothing on a bus.
- EDIT's settings are the project's MASTER settings; ARP and LFO have no settings ("NO SETTINGS"). HOME's settings
  are LOOP ENGINE's plus ADVANCE.
- Settings opened over a mode go back to that mode when closed (F1, F2 or the button).
- A mode tap acts on release; F1 / F2 stick one at a time and become momentary when used during their hold.
- PLAY while playing stops (not pause). REC toggles on release, since a hold opens the record settings.
- CROSSFADER: while A or B is held, K1-K4 are the cells of the param mode CROSSFADER was entered from (FILTER's
  otherwise), so a scene lock can reach a param. Glide times: instant / 150 ms / 600 ms / 2 s for the full travel,
  full pressure takes 0.15 of that. The base glide time, ADVANCE and INTRO live in the APP SETTINGS record.
- KEYBOARD: chromatic over all 27 keys on non-slice tracks (C4 = 0), octave -3..+3; live REC writes notes only when
  the target step is LIVE (a target step is step entry).
- PATTERN LAUNCH: SEL also picks the bank; stopped, a launch switches at once. Cells PLAYING / NEXT / QUANT / LENGTH.
- APP SETTINGS has an INTRO row; METRO SOUND is WOOD / BEEP / CLICK / COWBELL; F1 = back undoes, F2 = OK, HOME or any
  other button closes and keeps. It is saved in the flash header's sector (magic "ECAS").
- Gone with the old panel: ghost trigs by SHIFT + step (SHIFT + a white key selects a track), the hold-step +
  loop-key region lock (LOOP ENGINE's white keys play the loop; regions are still locked by recording loop keys), the
  slice-program copy / paste, the scope ops, the pattern-clear question and the scene strip.

**FILE, SAMPLER, recording**
- FILE's pages are BROWSE (B3), EDITOR (B4, a new fixed picture) and RAZOR (B5). In the EDITOR all black keys are
  zoom (B2-B10 = 1X .. 256X) or scroll (B1 / B11), so SEL changes the page. F2 frees the slot of the file under the
  browser's cursor. LOAD also points the selected track at the slot, and a file already in the pool is not loaded
  twice ("IN SLOT n"). Purge refuses while playing (STOP FIRST) or while a load / the WAV writer is busy.
- **FILE while playing opens the SAMPLER** (its other sub-mode, which is not gated) as well as flashing STOP FIRST;
  FILE from inside the SAMPLER is refused.
- SAMPLER: B1-B5 set the length FREE / 1 / 2 / 4 / 8 bars; the knobs are LENGTH, GAIN, MONITOR, FREE. Holding FILE
  (in FILE or the SAMPLER) opens 0.4's recorder setup (source IN / MASTER, threshold, sync, slot). These SAMPLER
  settings are runtime only. Takes keep the name rec-NNNN.wav; the slot carries the BPM.
- Live REC: no record mode clears anything; writes add or replace only what they write, and undo takes them back.
  CAPTURE differs only in how it starts (when stopped, the first key or loop key starts playback, no count-in); FIXED
  and OVERDUB start playback on REC after the count-in. FIXED ends bars x 4 beats after the start. The count-in only
  applies when REC starts the transport and clicks even with the metronome off. PLAY (stop) ends recording; entering
  FILE turns REC off. Automation skips sequencer, momentary and region params and the slot. Record settings add a
  CLICK LVL row (OFF, -36 .. 0 dB) and are saved with the project.
- Micro timing: EC_P_MICRO, 1/24 of a step, +-1/2 step, on TRIG's K4 ("+n/24"); the track value shifts every trig.
  An early step never fires before a queued pattern switch or the transport's start. Lock-only steps apply their
  locks for one step without a voice (existing locks-only steps now act too).
- A slot lock (hold a step + PRESETS) plays that slot's sample on the trig; a slot change declicks the track's voices
  over 128 samples, so alternating slots can cut a tail.

**Data, memory, graphics**
- The project record is version 2: sparse (params that differ from the defaults; steps and values packed), 64
  patterns, 128 slots, the record settings ("ECRS"). A dense 64-pattern project is ~275 KB (0.5's format: ~796 KB), so
  the flash image stays **1 MiB**; a project that doesn't fit is not saved ("FULL: NOT SAVED") and the old record is
  kept. Older records load into bank A and slots 1-16; a 0.5 firmware cannot read a 0.6 record.
- Memory at MEMORY_MIB 256: engine 18 MiB, the app (64 patterns) 9.4 MiB, memorize kept as a record (<= 1 MiB), the
  flash image 1 MiB, loader 1 MiB, stack 1 MiB, >= 4 MiB margin: the arena is **219 MiB** (~21.7 min of stereo), not
  224, because 64 patterns cost ~9 MiB.
- Graphics: ERROR / STOP texts use ALERT; a held step is shown by the header's count on LOCK and the LOCK cells (the
  old orange frame is gone); "locked on some step" is a LOCK outline. FX's INSERT page is the mock's slot list (the
  per-engine visuals of E4 were dropped). The envelope pictures light the stage of the knob turned last.
- Rate labels keep norns' names ("1", "2", "4", not "1X").

## 9. Planned for 0.6.1 (owner, 2026-11-03)

A Fantasy CFW runs only in hosts, so the device's limits don't apply. Two of 0.6.0's choices in section 8 change:

**Projects are files, with no size limit**
- A project is saved as a file in the cart's own folder, `/data/projects/` (the manifest already has `data=rw`), so
  its size is limited only by the disk. "FULL: NOT SAVED" goes away.
- The flash image keeps only the APP SETTINGS record and the name of the last project, which loads at power-on.
- SHIFT + SAVE saves over the current project's file. A new project gets the next free name (PROJ-001, ...).
- The FILE browser shows `/data/projects/` beside `/audio/`; LOAD on a project file opens it (FILE is already gated
  while playing).
- fs1 never blocks: the project is packed in memory, then written in chunks; the header shows SAVING until it lands.
  A failed write keeps the old file and shows the error.
- A 0.6.0 (or older) project in the flash still loads once; the next save writes it as a file.

**Two inserts per track**
- Each track has INS 1 and INS 2 in series. An empty slot costs nothing.
- FX's INSERT page lists both slots (close to the original mockup's slot list); SEL picks the slot, hold B1 picks the
  selected slot's engine. The SENDS / bus / FX MIX pages stay as built.

**Load meter, on the FX page only**
- A chunky bar on FX's INSERT page. It shows the cart's load: active voices, grains and effects, each weighted by its
  measured cost in wasm3. Full scale is the hosts' budget (2x real time on an M-series Mac).
- It is counted, not timed, so every host draws the same screen (RFM1-FORMAT: the screen must not depend on the host
  clock).
- No warning or pop-up anywhere; the host's own "too heavy for this device" stays.

## 10. Owner's play-test of 0.6.0 (2026-11-03): fixes for 0.6.1

**Rules for every screen**
- **No mode pop-up.** Entering a mode shows nothing extra; the APP SETTINGS row MODE POP-UP goes away.
- **No pop-ups for messages.** Messages go in the header (NOT MODDABLE, STOP FIRST, SAVING, a moved step, ...). The
  one exception is the FX type menu below, shown only while its key is held.
- **The header always says what a held key does**, for every key that does something while held: mod keys, steps,
  F1 / F2, SHIFT, CROSSFADER's A / B and B11 (capture), hold-for-settings buttons, the insert key. It returns when
  the key is let go.
- **No 1-pixel details**, including the modulation depth bar on the param cells. It is drawn at least 2 units of
  the 120 grid. The `grid` test must cover the frames that showed the 1-px bar: params with modulation, a held mod
  key, a held step with locks.

**Modulation keys**
- A tap or hold on a source key (B6-B10) **never changes the mode**. In FX you stay in FX.
- While a mod key is held, the header shows the source's name (LFO 1, LFO 2, MOD ENV, VEL, AT, M1-M4) on a **blue**
  header. Every depth bar from that source is drawn blue, and turning a knob sets its depth.
- Blue is the SRC role: SRC becomes each theme's blue (it was the accent orange). Every theme keeps it distinct
  from ACC, LOCK and ALERT.
- A param that can't be modulated shows **NOT MODDABLE** in the header: no message box.

**FX**
- Holding the insert key opens **a menu of the effect types** while it is held: SEL or K1 moves through it, letting
  go picks the type.
- **K4 is always the effect's MIX**, on every effect type.
- An effect with more than three other params gets more knob pages: a tap on the insert key again turns the page
  (header P1 / P2), so every param is reachable.

**Step locks: three states on every param cell**
- **Holding a step, param locked on it:** the **whole cell** (label and value, the full box) fills with LOCK, and
  the value is the lock's.
- **Not holding a step:** the cell shows the track's value.
- **Not holding a step, and some step of the pattern locks this param:** a 2-unit LOCK outline around the whole
  cell.

**CROSSFADER**
- Holding B11 says CAPTURE in the header (then A or B captures the scene).

**KEYBOARD**
- **While stopped, a key plays the sound**: the loop (or slice) at that key's pitch, for as long as it is held.
- **Pitch direction:** a positive PITCH moves the highlighted key up the keyboard, a negative one down (it was
  reversed).
- Moving the target step (ALGO) shows no pop-up: the highlighted step is enough.
