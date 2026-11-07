# Elasticat FM-1 0.6.0: modes and key shortcuts (spec, 2026-11-03)

The owner proposed this on 2026-11-03 and settled it in six rounds of questions, with one mockup per mode.
**Status: built in 0.6.0** (2026-11-03); what the build decided where this spec is silent or could not be followed is
in §8. Sections 9 and 10 are **built in 0.6.1** (2026-11-03); that build's own decisions are in §11. The panel as built: docs/FM1-CONTROLS.md.

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

**LOOP ENGINE quick settings** (owner, 2026-11-03)
- Holding a page key opens a quick menu for one setting, the same kind of menu as FX's type menu: it shows while the
  key is held, SEL or K1 moves through it, letting go picks.
  - B1 (LOOP) = loop mode
  - B2 (MACHINE) = machine
  - B3 (WARP) = warp mode
  - B4 (RANGE) = voice mode
  - B5 = ADVANCE
- A tap still turns to the page.

**List menus** (owner, 2026-11-03): APP SETTINGS, the record settings, every mode's settings and any other list
- SEL scrolls; **one row is selected**, and **K1 changes the selected row's value**. Rows are not mapped to the
  knobs in groups of four.
- A row whose label and value don't fit on one line uses two: the label, then the value on the second line (THEME
  first: the theme's name goes on the second line, so longer names fit).

**FILE / EDIT trim with the white keys** (owner, 2026-11-03)
- The white keys are 16 places across the visible waveform (so the zoom sets how fine they are).
- **F1 = place START, F2 = place END**: it picks which point a white key places. The header shows which.
- A tap on a white key places the chosen point there.
- **Two keys set both points**: hold a white key to place the chosen point, then press a second white key to place
  the other one. In START, the held key is the start and the second the end; in END, the reverse.
- F1 / F2 have only this job in the EDITOR (no copy / cut there).

**FILE pages and RAZOR** (owner, 2026-11-03)
- In FILE, **SEL always turns the page** (BROWSE, EDITOR, RAZOR, both ways), RAZOR included, so there is always a
  way back.
- In RAZOR, **ALGO selects the slice** (it was SEL).

**FILE / POOL page** (owner, 2026-11-03): one place to see and manage all 128 slots
- A new FILE page, POOL, on a free black key of FILE (the build picks it and the pop-up / key labels name it). SEL
  still scrolls the list here (like BROWSE).
- A list of all 128 slots, empty ones included: the slot number, the file's name (or a dash), its length, and which
  tracks play it. A slot only used by step locks is marked too. The header shows the pool meter (memory used / free,
  slots used).
- SEL moves through the slots. Moving onto a used slot previews it (APP SETTINGS > AUTO-PREVIEW); B11 plays / stops.
- B10 **ASSIGN**: the selected track plays this slot.
- F1 **MOVE**: F1 picks the slot up, SEL to another slot, F1 again drops it there (swapping with whatever is there).
  Every reference follows: the tracks' slots and the slot locks in all 64 patterns. Undoable.
- F2 **FREE** the selected slot (its tracks fall silent), as in BROWSE. SHIFT + B11 purges, as in BROWSE.
- Like the rest of FILE, it can't open while playing.

**MIX shows each track's pan** (owner, 2026-11-03)
- Every strip on MIX (T1-T6, S1, S2; MST if it has a pan) shows its pan under the level bar: a short track as wide
  as the strip with a marker on it, the centre marked, the marker left / right of centre with the pan. A p-locked or
  modulated pan shows where it is now.
- 120-grid rules: the track and marker are at least 2 units; the selected track's marker in ACC.

**CAT stage** (owner, 2026-11-03)
- The CAT's background is black (the sprite's own background colour becomes BG; the cat's outline stays readable).
- ~~Animated stage light beams~~ (replaced, owner 2026-11-03): **a generative fractal behind the cat**, in the
  theme's grey ramp only (L3, L5, L8, L12, L15 on BG), to match the greyscale cat. It moves with the music: the
  fractal's shape drifts once per bar and pulses on the beat. Stopped, it drifts slowly. Drawn in 2-unit blocks,
  never over the cat; computed from the transport position, never the host clock.
- **Full screen**: the CAT has no header, track strip or knob cells around it; just the stage, the fractal and the
  cat, scaled as large as the 120 grid allows. B11 again (or any mode button) leaves it.

**SHIFT held = SHIFT sticky** (owner bug, 2026-11-03)
- Holding GLO shows the SHIFT screen and everything on it (BPM, SWING, TRACK, the shortcut keys, the knobs) works
  exactly as with sticky SHIFT, for as long as GLO is held. Letting go returns to the mode.

## 11. Built in 0.6.1

Sections 9 and 10 are built as written. Where they were silent, the build chose the simplest thing consistent with
them:

**Header, pop-ups, held keys**
- The mode pop-up and its APP SETTINGS row are gone. The APP SETTINGS record keeps its layout and version 1 (the old
  pop-up byte is ignored and written 0), so a 0.6.0 record loads with every other value.
- Every message is the header's line for 1.2 s (alerts on ALERT). A message newer than the last key press wins over a
  held key's text; an older one gives way. Value takeovers (PRESETS' slot, TAP, a knob whose value is off screen) stay:
  they show a value, not a message.
- A held key's text appears 150 ms into the hold (a tap shows nothing); sticky SHIFT / F1 / F2 show at once. Bars:
  sources SRC, held steps LOCK, alerts ALERT, the rest the plain bar. Texts: the source's name; STEP n / n STEPS;
  COPY / CLR LOCK and CUT / CLR MOD (PASTE once the buffer is full; COPY PAGE / PATTERN in PAGES / PATTERN LAUNCH);
  SHIFT; FILL; FX SETUP, FILTER SETUP, AMP SETUP, PROJECT SETUP, LOOP SETUP, RECORDER SETUP, F3, REC SETUP for the
  settings holds; CAPTURE, SCENE A / B, GLIDE TO n in CROSSFADER; NOTE C4 / SLICE n in KEYBOARD; LOOP KEY n / PAD n;
  LOOP PAGE n; MUTE / UNMUTE Tn; the hold menus' INS 1 TYPE, S1 TYPE, LOOP MODE, MACHINE, WARP MODE, VOICE MODE,
  ADVANCE; START + KEY = END in the EDITOR. Keys whose hold does nothing beyond the tap say nothing.
- Source keys never change the mode. A tap opens the source's page only when it is in the mode shown (MODULATION: LFO
  1, LFO 2, VEL, AT = the MATRIX, M1-M4 = that macro; AMP: MOD ENV); elsewhere a tap does nothing. F2 on an
  unmoddable param also says NOT MODDABLE; F2 on a moddable param with no route says NO MOD; a full matrix is the
  alert MATRIX FULL.
- Held SHIFT is sticky SHIFT for the whole hold: its screen shows from the press, its knobs and shortcuts work; a used
  hold never turns sticky, an unused tap does. The SHIFT knobs stay as §1 says (SEL = BPM, ALGO = swing, PRESETS =
  the track); K1..K4 with SHIFT snap.

**Graphics**
- SRC is each theme's blue: CREAM 3F8CFF, AMBER 5A96FF, PHOSPHOR 4A86FF, DMG 5890E8, ICE 3C64FF, PAPER 1F4FD0 (at
  least 150 from ACC, LOCK and ALERT in RGB distance; a test checks it).
- **BG is pure black: the FM-1 LCD lifts near-black.** Every colour with R, G and B all <= 0x20 is 0x000000 (CREAM,
  AMBER, PHOSPHOR and ICE BG and SHADE, PAPER L15, DMG BLACK; ICE's SHADE 0B1A2E too). The pop-up dim map now dims to
  L3, and the CAT's ground is a transparent key, the two places SHADE was drawn. A test checks no colour has all
  channels in 1..0x20.
- No detail on any screen is one grid unit: every bar, line, marker, tick, outline, rule and gap is >= 2 units (text
  and the owner's sprite art excepted). Cell bars sit on a 2-unit lattice (27 cells); a tiny depth is one 2x2 cell.
  The cell rules are 2 units, the row-98 rule is a 2-unit gap, the boxes are 61 x 23. Filter / envelope pictures are
  29 bars with 2-unit gaps (was 38 with 1-unit gaps), waveforms 2-unit columns, playheads 2 wide; PATTERN LAUNCH, the
  SHIFT keys and ghost steps became filled blocks where a 2-unit frame would cut the name.
- Why `grid` missed the 1-unit bar: it checked that every 2x2 LCD block is one colour, which a 1-grid-unit bar passes
  by construction, and no frame had a route, a held source or a depth lock. It now checks every frame for any
  non-ground run exactly 1 unit thick and >= 3 long (text and sprites masked by draw-call tracking), with frames for
  modulation, held sources, the three lock states, the hold menus, the lists, POOL, MIX's pans and the CAT's fractal;
  `check_png.py --survey [--strict]` lists hairlines in PNGs.
- Lock cells: with several steps held, the lock of the first held step that has one; a held step without the lock
  shows the plain track value (no outline); a filled cell draws its label and value a unit higher, so 2 units of LOCK
  separate the value from the bar. A held source's depth cell uses the same three states for its depth param. While
  a source is held its depth bars are SRC on the param cells, the MATRIX rows, MDEP cells and single-macro rows.
- MIX: a pan track (10 x 2, centre marked in the track) under each strip, the 2 x 6 marker in 4 steps a side, the live
  value (the playing lock and the crossfader plus the live modulation); the selected marker ACC, muted L5. S1 / S2 have
  no pan in the engine: their marker rests at the centre in L5; MST shows none.
- CAT: full screen, the cat at 1x centred (its art spans 64 x 61 units, so 2x does not fit), its outline in BLACK so it
  reads on PAPER and DMG too. While playing the dance follows the beats (5 frames a beat). The cat art's own
  8-unit joystick stroke is left as drawn.
- CAT stage, 0.6.2 (replacing 0.6.1's four colour beams): a Julia set on the 60 x 60 grid of 2x2 cells, never on a
  cell that touches the cat. c goes once round the edge of the Mandelbrot set's main cardioid (c = e^it/2 - e^2it/4:
  always a connected set, from a round blob to dragon shapes) once a bar, from the transport's beats; each beat zooms
  the view in 12 % and eases out, and for the first quarter of the beat the bands step one grey on. Stopped, c goes
  round once every 1280 CAT redraws (~38 s). A cell's grey is its escape count (cap 10) on a triangle over the ramp,
  BG L3 L5 L8 L12 L15 L12 L8 L5 L3, so the bands are smooth contours; the inside is BG, so the cat stands in the
  set's dark middle. Q12 integer maths; the set's point symmetry means half the cells are computed. Cost: ~0.18 ms
  a redraw in wasm3 (M-series, rfm1_run), ~0.6 % of a core at the CAT's ~33 redraws a second; the heavy project's
  speed with the CAT up is unchanged (2.0-2.1x on a loaded machine, the same with it off).

**FX**
- INS 2 = new params `ec_fx_wet` (227), INS2 MACHINE (228), INS2 MIX (229) and INS2 P1..P8 (230..237), mapped by index
  onto the machine's params in the layout's FX/INSERT order; stored in the mapped param's units, reset to the
  machine's defaults on a type change. EC_NP is 238 (ids stay u8).
- K4 = MIX on every type: DESTROY, DJ EQ, DUCK, MOTION and LIMIT had no dry / wet, so K4 is the new `fx_wet` (default
  fully wet: old projects sound the same).
- The INSERT page: SEL picks INS 1 / INS 2; a tap on B1 there turns the knob page (P1 / P2 / P3 in the header box). The
  slot and the knob page are global (not per track); the page goes back to P1 when the slot or type changes.
- An empty slot is skipped whole (no processing, its state untouched); the POST send tap is after both inserts.
- The engine is 25.6 MiB (EC_DSP_MEM_MAX 26), the project file's pack buffer 11 MiB: the arena is **200 MiB** (~19.8
  min of stereo) at MEMORY_MIB 256.
- The load meter (FX INSERT page only, a bar and a percentage): counted from the engine's own state after each block
  (the base, voices by warp mode, slice voices, grains, harmonizer voices, the mod engine, every running FX by
  machine, both inserts, the buses), weights measured in wasm3 with rfm1_run (src/dsp/ec_load.c), smoothed over
  ~0.25 s; 100 % = 2x real time. It reads 0.95 on the heavy project (measured 0.98-1.03) and 1.31 with two heavy
  inserts on every track (measured 1.33).

**Hold menus** (one component, src/app/ec_holdmenu.c)
- Shows after 400 ms, or at once when SEL or K1 turns; SEL or K1 moves; the release picks, only if the cursor moved;
  each pick is its own undo step. FX: B1 = the selected insert's type, B3 / B4 / B5 = the S1 / S2 / MST type.
- LOOP ENGINE: B1 = loop mode (the slice play mode on slice machines, else the chop loop mode); B2 = machine (NEIGHBOR
  skipped on T1); B3 = warp mode (STOP FIRST while playing, as the knob); **B4 = voice mode, mapped to
  `trig_polyphony`**, the param LOOP ENGINE's setup already calls VOICE MODE; B5 = ADVANCE, hold only (a tap does
  nothing).

**Lists**
- One list (ec_kit_list): the selected row is an L3 band with a 2-unit ACC edge; rows 14 units, two-line rows 24 (the
  value right-aligned on line 2); the window keeps the selected row and the next in view. K2..K4 do nothing in a
  list; the live labels say SEL = ROW, K1 = the row's name. The recorder setup is a list too (SOURCE, LENGTH,
  THRESHOLD, SYNC, SLOT); while a take is armed or running only THRESHOLD changes. A list opens on its first row.
  F3 stays a cells page.

**FILE**
- Page order BROWSE, EDITOR, RAZOR, POOL. SEL turns the page in the EDITOR and RAZOR, both ways (it first steps
  through their two knob lanes); in BROWSE and POOL SEL scrolls the list, and the page keys leave them (B3 BROWSE, B4
  EDITOR, B5 RAZOR, B6 POOL). A tap on FILE still goes to the SAMPLER.
- EDITOR: F1 START / F2 END (the header shows which, the chosen marker in ACC). Each tap is one undo step, a two-key
  gesture one too; the START / END knobs are undoable now. Swaps keep a gap of 0.005.
- **POOL is on B6.** 12 rows (no knob cells). Track marks T1..T6 are for the playing pattern's tracks; the lock-only
  mark covers step locks in any pattern. MOVE picks up only used slots; dropping on an empty slot moves it; it
  rewrites the tracks' slot params, slot and slice locks in all 64 patterns and the runtime slots, one undo step.
  Memorize is not rewritten (a recall restores its own pool layout); scenes hold no slots.

**Projects as files**
- A project is `/data/projects/PROJ-nnn.ecp`: exactly the version 2 record (magic, version, length, checksum, then the
  record). "Next free" is the lowest unused number; a new project is named at its first save (the autosave included).
- No rename in fs1, so a save writes a hidden `.NAME.ecp`, then `NAME.ecp`, then deletes the hidden copy; a load takes
  the main file if its checksum holds, else the hidden copy. A failed write leaves one whole copy and says NOT SAVED:
  DISK FULL / IO ERROR / NO FOLDER in ALERT. A save packs in one tick into a static 11 MiB buffer (the largest
  possible project measures 11.25 MB) and writes 256 KiB a tick; SHIFT + SAVE shows SAVING, then SAVED; autosaves are
  quiet.
- The flash keeps APP SETTINGS and an "ECLP" record (the last name), written once a file save lands or after a LOAD;
  power-on reads the named file (waiting for the folder if it isn't allowed yet; PROJECT MISSING / DAMAGED starts a
  fresh project). With no folder and no name, a save falls back to the 0.6.0 flash record (<= 1 MiB). The flash image
  stays 1 MiB, so old images load; a 0.6.0 or 0.5 record loads once and the next save makes the file.
- FILE opens in /audio; B1 from /audio goes to a top level with AUDIO/ and PROJECTS/. PROJECTS/ lists the .ecp files
  (the open one marked); LOAD saves unsaved edits first (a failed save cancels it), releases every sample, reloads the
  slots, clears undo and goes to LOOP ENGINE. Project files never preview. The E5 project name stays inside the file.

**KEYBOARD**
- Stopped, a key plays the whole loop region from its start at its pitch until the last key is let go (slice tracks:
  the slice, as before); the lit key is the note sounding (key + PITCH), and PITCH turned while a key is held is heard.

**Not done**
- Memorize still holds at most 1 MiB (MEMO FULL on a bigger project).
- The intro logo's art has 1-unit streaks of its own; the CAT's joystick stroke too. Both are the norns art as drawn.

**One page list per mode: SEL and the page keys agree** (owner, 2026-11-03)
- Every mode has **one ordered list of its pages**, every knob page included (lanes, P1 / P2, INS 1 / INS 2, the
  bus pages). In FX: INS 1 P1, INS 1 P2.., INS 2 P1.., SENDS, S1.., S2.., MST.., FX MIX.
- **SEL walks that whole list, both ways**, from any page; it stops at the ends (no wrap). Turning back always
  returns the way it came.
- **A page key jumps into the list**: its first page, and tapping the same key again steps through its group's
  pages (the same ones SEL passes through). Nothing is reachable only by a key.
- This replaces "SEL picks the slot" on FX's INSERT page (INS 1 / INS 2 are pages in the list). Lists (BROWSE,
  POOL, the settings lists, the MATRIX rows) keep SEL for their rows; there the page keys move between pages.
- The header always shows the page and its place (e.g. INS 2 · P2).

**0.6.2 navigation** (built 2026-11-03, the owner's "FX menu is very inconsistent")
- One model, `src/app/ec_nav.c`: `ec_nav_list(mode)` builds the mode's list from the layout and the state now (a
  page's lanes, INSERT's knob pages per slot, the bus pages, PATTERN LAUNCH's banks); SEL (`ec_nav_sel`), the page keys
  (`ec_nav_key*`) and the lit mode button (`ec_nav_next_group`) all move along it, so they cannot disagree. The state
  it reads and writes stays where it was (the page, the lane, A->bus, ins_slot / ins_page, the bank).
- The bug: SEL on INSERT picked the slot and could not leave the page, the bus keys B3-B5 put FX on a bus that SEL then
  walked as the track's pages, and the INSERT knob pages were reachable only by B1. Now FX is INS 1 P1, INS 1 P2 ..,
  INS 2 P1 .. (B1), SENDS (B2), S1 .. (B3), S2 .. (B4), MST .. (B5), MIX (B11): the buses go before the first page
  keyed above theirs; SEL from S1 back to SENDS selects the track again.
- A key outside its group jumps on press (so a hold menu then edits that page's slot / bus); inside its group it
  steps on release, and only for a tap (a hold menu shown or a knob turned during the hold: no step). Past the
  group's last entry it goes back to the first. A key's jump keeps the MATRIX row; SEL onto it takes its near row.
- The lit mode button goes to the next group's first page and wraps (as the button always cycled): FX INSERT →
  SENDS → S1 → S2 → MST → MIX → INSERT (it used to skip the buses and leave a bus for INSERT).
- SEL stops at both ends in every mode (it wrapped before, so turning back from the first page jumped to the last).
- Lists keep SEL for their rows: BROWSE and POOL scroll and are walls (SEL stops on them; FILE's EDITOR / RAZOR
  turn onto them, BROWSE first, POOL last, so the FILE rule of §10 holds); the MATRIX walks its rows, then on past
  either end (at the list's start its row stays). PATTERN LAUNCH: SEL and B1-B4 both pick the bank, now through
  the list.
- Exemptions from "every page has a key": MIX (its black keys are the rates; the page follows the target), the
  EDITOR's second lane (its black keys are zoom / scroll). Both are reached by SEL. With a bus as the target, the
  modes that show the bus page list only its lanes, and their page keys do nothing.
- The header's page box shows the place among the page's entries (P1, P2 ..) when it has several (lanes, INSERT's
  knob pages, a bus's lanes), else the page number as before: FX/INS 2 · P2.
- A quick-setting key (FILTER / AMP's B1) no longer marks its own hold as used when it goes down, so its tap steps
  its group like the other page keys.
- Test: `host/test_nav.c` (ctest `nav`): every mode's list walked by SEL to the end and back, every page key tapped
  from every page (it lands in the list; a jump lands on its group's first; taps step exactly its group), every entry
  reached by a key (bar the exemptions), SEL on both ways after a key jump; FX with two-page inserts and a two-lane
  bus, the header's place, a bus target.

**CAT fractals: always evolving, a new one each time** (owner, 2026-11-03; replaces the single Julia loop)
- **Each time the CAT opens it picks a fractal algorithm at random** from a set of several (e.g. Julia sets,
  Mandelbrot zooms into its detailed edges, Burning Ship, Tricorn, higher-power Multibrot / Julia, Phoenix, Newton),
  never the same one twice in a row. The randomness is the cart's own generator, seeded from the tick count, never
  the host clock.
- **It never settles into a short loop**: its parameters drift along paths that don't repeat (several slow motions at
  unrelated speeds: the constant, the zoom, the centre, rotation), so the picture keeps changing for as long as the
  CAT is open. Zooms stay inside detailed regions and recover before running out of precision.
- The music still drives it: faster drift while playing, the beat pulse and bar accents as now. Stopped: slow drift.
- Same rules as before: the theme's greys only, 2-unit cells, never over the cat, cheap enough to cost audio nothing
  measurable.

**FX layout: one effects page, a MASTER page** (owner, 2026-11-03; replaces the INSERT / bus pages and B1-B5 above)
- **The EFFECTS page** lists four rows, drawn like today's FX/SEND page: INSERT 1, INSERT 2, SEND 1 (S1's effect),
  SEND 2 (S2's effect). Each row shows its effect's name and level bar; the selected row is highlighted. The knob page
  boxes (P1 / P2 ..) show for every row, as on FX/SEND today.
- **B1 = INSERT 1, B2 = INSERT 2, B3 = SEND 1, B4 = SEND 2.** A tap selects that row; tapping it again steps its
  knob pages (P1, P2 ..). Holding it opens that slot's type menu.
- **B5 toggles SENDS / MASTER.** SENDS is the track's send levels (the old B2 page). **MASTER** is its own page: the
  master effect (COMP) and its knob pages, and **the load meter** (moved here from the INSERT page).
- B11 stays FX MIX. K4 is MIX on every effect (inserts, sends, master).
- SEL walks the one page list: INSERT 1 P1.., INSERT 2 P1.., SEND 1 P1.., SEND 2 P1.., SENDS, MASTER P1.., FX MIX.
- **SENDS page knobs: K1 = INSERT 1, K2 = INSERT 2, K3 = SEND 1, K4 = SEND 2.** The inserts sit in series, so their
  "send" is each insert's MIX (the same param as K4 on its effect page). The picture gains two bars, I1 and I2, above
  S1 and S2, each with its effect's name, and the chain shows both inserts.
- **SENDS shows the track's full routing**: sample → FILTER → INSERT 1 → INSERT 2 → AMP (level, pan) → MIX, with S1
  and S2 tapping off it to their effects and on to MASTER. Each block shows its effect's name; the send taps are
  drawn where the send mode puts them.
- **ALGO on SENDS = the send mode, PRE / POST** (per track, both sends): PRE taps after the filter, before the
  inserts (today's routing); POST taps after the inserts and the track level and pan. Saved and p-lockable like any
  track param, undoable.
- **Holding a black key on SENDS reorders instead of opening the type menu**: hold B1 or B2 = swap INSERT 1 and
  INSERT 2 (their order in the chain: machines, params, locks and mod routes move together); hold B3 or B4 = swap
  SEND 1 and SEND 2 (the two bus effects with their settings, and every track's send levels and locks swap with them,
  so the sound is unchanged). The header names the swap; undoable. On every other FX page, holding a black key still
  opens the type menu.
- **FX settings (hold FX / SHIFT + FX) list every slot**: INSERT 1, INSERT 2, SEND 1, SEND 2, MASTER (each row picks
  that slot's effect type), then SEND MODE.

**Built in 0.6.3** (2026-11-03)
- CAT stage, 0.6.3 (`src/ui/ec_cat_fractal.c`, replacing 0.6.2's single Julia loop): each B11 entry picks one of seven
  algorithms with the app's xorshift32 (`pn.cat_rng`, stirred with the tick count and the redraw count at entry; never
  the host clock), never the previous one (`ec_scr_cat_next`): **JULIA** (z^2 + c; c glides between well-loved
  constants -- dendrites, rabbits, spirals, Siegel disks, dust -- a new random one every ~22 units, a small wander on
  top; point symmetric: half the cells), **MANDEL**, **SHIP** (Burning Ship), **TRICORN** (Mandelbar) (the zooms),
  **JULIA4** (z^4 + c, c anywhere on the quartic Multibrot's edge, a little out; half the cells), **PHOENIX** (z^2 + p
  + q z', p and q wandering), **NEWTON** (z^3 - 1 with a wandering complex relaxation; the grey from the root and the
  steps). The evolution clock `pn.cat_t` (counted from the entry): stopped, 1/40 a CAT redraw (~0.83 a second);
  playing, 1.5 a beat of the transport (3 a second at 120 BPM). Everything wandering (the constants, zoom, rotation,
  centre) is a sum of three sines at unrelated rates (x1, x sqrt 2, x golden ratio) with the entry seed's phases, the
  rotation with a steady turn on top (never turning back): nothing comes round again.
- The zooms: t is cut into epochs of 420 units, each into six cycles of random lengths; a cycle dives from the
  overview to a target and pulls back out quickly (the overview is the same view for every target, so cycles join
  seamlessly), the target moving out beside the cat (0.62 of the half width, its angle wandering). The target is
  found once a cycle (in doubles, cached): from one of the set's detailed regions (MANDEL: the valleys, bulbs and the
  overview; SHIP: its antenna's little ships; TRICORN: its three tips), level by level into the third of a 12 x 12
  probe grid with the most detail (distinct bands, jumps between neighbours that are not a gradient, some inside
  but not mostly), to a random depth (MANDEL up to 2^12.5, SHIP 2^11, TRICORN 2^8; Q28 in 64 bits keeps over 2000
  steps of precision across a cell there). The iteration cap rises with the depth (16 + 4 a zoom octave, at most 64)
  and the bands widen (1 + (cap - 16) / 20 counts a band; SHIP one more: its antenna glitters).
- Detail check: a frame whose biggest grey class covers over 86 % of the stage, or whose BG (the inside) over 60 %,
  pulls the view back (0.06 octave a frame, 0.5 when flat), easing in again by 0.01 a frame once it has detail; the
  zooms never pull past the overview, the Julia kinds at most 1.5 octaves.
- Cost control: JULIA and JULIA4 (point symmetric, centred on 0) compute half the cells through the mirror; the others
  compute half the cells a frame (a checkerboard, alternating; the other half is the last frame's count, kept as its
  band), all of them after a new algorithm, seed or a jump in t.
- The beat: the zoom pulses in 10 % on a beat, 20 % on a bar's first, easing out over the beat; the bands step one
  grey on for its first part (two on the bar).
- Cost, native (M-series, -O2, mean / worst a redraw over ten minutes of evolution): JULIA 0.04 / 0.08 ms, MANDEL
  0.07 / 0.17, SHIP 0.08 / 0.16, TRICORN 0.07 / 0.14, JULIA4 0.06 / 0.09, PHOENIX 0.04 / 0.06, NEWTON 0.09 / 0.14.
  wasm3 (rfm1_run on the heavy project, the CAT forced up with a fixed algorithm and drawn 11 times a redraw against
  once; 785 redraws in 20 s, user time, two runs): JULIA 0.32 ms, MANDEL 0.51, SHIP 0.60, TRICORN 0.50, JULIA4 0.32,
  PHOENIX 0.32, NEWTON 0.44 a redraw (before the half-a-frame update the zooms cost 0.77-0.95 ms). At the CAT's ~39
  redraws a second that is 1.2-2.3 % of a core. The heavy project's speed on the same (loaded) machine: 2.1x with the
  CAT off (10.6-10.7 s user for 20 s), 2.0-2.1x with it up (10.8-11.2 s: JULIA4 / PHOENIX / JULIA / NEWTON 2.1x,
  the three zooms 2.0x).
- FX layout (§10 "FX layout"): the slots 0 INSERT 1, 1 INSERT 2, 2 SEND 1, 3 SEND 2, 4 MASTER (`ec_fx_slot`); a bus
  slot is edited through the bus target (A->bus 1..3), so targeting S1 / S2 / MST with the track keys shows that row /
  MASTER in FX. Every slot's knobs come from the layout's FX/EFFECTS variant for its machine (`ins_list`): K1-K3 in
  threes, K4 its MIX (`ec_src_ins_mix`: fx_mix, or fx_wet without a blend; INSERT 2: its own MIX). The page list
  (`ec_nav.c fx_list`): INSERT 1 P1.. (B1), INSERT 2 P1.. (B2), SEND 1 P1.. (B3), SEND 2 P1.. (B4), SENDS, MASTER
  P1.. (B5), then the layout's others (FX MIX, B11). B5 steps SENDS -> MASTER P1 -> MASTER P2 -> SENDS (one group, so
  the generic key rule holds; COMP has two knob pages). The FX button tapped again goes to the next row / group.
  Layout: FX/INSERT is now FX/EFFECTS (B1), SENDS moved to B5, the MST bus has no key (S1 B3, S2 B4 as before).
  Other modes with a bus target keep the bus's layout page, its cells put in the same K1-K3 + K4 MIX lanes
  (`ec_page_bus_k4`, applied by the app so the layout's pages and the 0.4.0 hash stay).
- SENDS (`ec_fx_sends_items`): K1 INSERT 1's MIX, K2 INSERT 2's MIX, K3 SEND 1, K4 SEND 2; ALGO = `send_tap`, the
  existing per-track send mode (saved since E4; 0.6.3 makes it p-lockable), PRE (1, the default: old projects keep
  it) / POST (2). In the DSP (`track_chunk`, unchanged): the track's level and pan act at the filter's output
  (`ec_filter_process`), so PRE taps after the filter, level and pan, before INSERT 1; POST taps after INSERT 2; the
  mute is after both. That is the spec's POST (after the inserts and level / pan); the picture draws AMP after the
  inserts, as the spec does, though the DSP applies it before them. No DSP change: no cost. Test: tests/dsp test_v61
  "the send mode" (an insert killing every band: POST sends ~nothing, PRE the loop).
- Swaps (`ec_project_swap_inserts` / `_sends`, `EC_U_FXSWAP`: the undo applies the swap again): the inserts by an id
  map per pattern (each pattern's own machines): INSERT 1's params in the layout order <-> INSERT 2's P1.., the
  MIXes, the machines; values, step and slice locks, matrix destinations and the scenes' values follow. The sends: the
  two bus records, SEND 1 <-> SEND 2 on every track (values, locks, routes, scenes) and the scenes' S1 / S2 rows. A
  held B1-B4 on SENDS arms `EC_HM_SWAP` (the header names it, "SWAP INSERTS" / "SWAP SENDS"; at 400 ms it swaps,
  once; released before: a tap, which goes to that row).
- FX settings (`ec_fx_setup_items`): INSERT 1, INSERT 2, SEND 1, SEND 2, MASTER, SEND MODE; the row of the place the
  knobs edit is its own param (INSERT 1's fx_insert1_machine, or the targeted bus's), the others panel items
  (EC_UX_FXT0 + slot; INSERT 2's resets its params and is one undo; a bus's is global, no undo, like its knobs).
- Screens (`ec_scr_e4.c`): EFFECTS rows 14 units, the knob pages as 4x2 boxes under each row's bar; MASTER: its row and
  the load meter; SENDS: the routing in two rows of blocks (effect names abbreviated: the option labels), the tap in
  ACC, then I1 / I2 / S1 / S2 with their effects' names and bars.
- Tests: v61gfx (each algorithm: greys only, detail every minute of 30, no frame twice in 3600, frames two minutes
  apart differ; 24 entries never repeat an algorithm and reach at least five; playing evolves faster), grid (the
  kit's CAT frames over every algorithm), fx2 (bus K4 MIX on every page, SENDS cells, ALGO send mode and its undo, both
  swaps with undo and a lock and a route following, the FX settings rows), nav (FX's list and keys), ux / v61ux /
  v61list (keys, headers, the settings list), real screens Fx_send1, Fx_sends, Fx_sends_post, Fx_master, Fx_setup.

**SENDS: short boxes, raise and move** (owner, 2026-11-03; replaces "holding a black key on SENDS swaps")
- The routing boxes say **FX 1, FX 2, SEND 1, SEND 2** (owner, second pass; the effect names stay on the level bars
  below show only the effect names). The orange send path is one 2-unit line with square corners, its run ending
  flush with the outer drops.
- Holding B1-B4 on SENDS **raises** that box (lifted 2 units, bright, a shadow where it sat) without moving it; the
  header says MOVE and the effect's name. **K1, still holding, moves it** left or right; each step swaps the pair
  (inserts: everything on them; sends: the buses and every track's send levels, the sound unchanged). Letting go puts
  it down: one undo for the whole move; let go without K1 and nothing changes.
- While held, each box keeps the name it had when the key went down (FX 1 moved right still says FX 1), and a
  move slides the boxes past each other over 160 ms (device time) instead of jumping.
- Built in 0.6.4 (by hand): ec_holdmenu.c's EC_HM_SWAP (ec_hm_raised), ec_fx_swap_do / ec_fx_swap_undo in
  ec_srckeys.c, slot_block in ec_scr_e4.c; test_fx2's test_swaps; the real shot Fx_sends_move.

**Locks like Elektron** (owner, 2026-11-03; replaces "a lock holds while its note lasts")
- **Sticky**: a step's lock holds after its note, until a trig or lock changes that param. A trig without the lock
  puts the track value back first (its own locks then apply); a ghost trig (no new note) carries the values on. A
  lock-only step (a trigless lock) sets only what it locks. The owner's case: track cutoff 20 kHz, step 1 locked to
  15 kHz (one step long), step 2 empty, step 3 locked to 5 kHz: step 2 stays at 15 kHz.
- **The knob takes over**: turning a track value by hand while a lock holds it is heard at once (until the next
  trig or lock).
- Region locks (loop start / end) keep their note window; the slot lock already worked this way.
- **SLIDE** (new param `trig_slide`, TRIG lane 2 K1, OFF; a step lock): on a step with SLIDE on, each sound param it
  locks ramps linearly to the lock of the next step that sets that param (a trig without that lock: no slide),
  arriving on that step's grid boundary; the look-ahead follows the play order. Resent every audio chunk while
  sliding.
- Built in 0.6.5 by hand: ec_seq.c (hold_set, holds_clear_sound, start_slides, ec_seq_param_edited), EC_P_SLIDE;
  tests: test_seq's test_sticky (the owner's case, trigless lock, slide, the knob), e5 / seq_e4 / rec / mtx updated.

**LOOP ENGINE step entry: sequencing regions and ranges** (owner, 2026-11-03)
- The norns grid had a step row and a loop-key row at once (hold a step, tap loop keys). The FM-1 has one row, so
  LOOP ENGINE gets KEYBOARD's **target step**:
  - **ALGO = the target step** (shared with KEYBOARD); below step 1 is **LIVE** (the white keys scrub the loop live,
    as now). The header says STEP n.
  - **B11 replaces the zoom**: hold B11 + a white key = jump the target to that step (of the step page shown);
    tap B11 = LIVE. The 32-key zoom moves to SHIFT + B11.
- **With a target step, the white keys write it**: hold one or more keys; letting go of the last one writes their
  span as the step's region lock (one key: that division; several: first..last) and puts a trig on it, then the
  target moves on (KEYBOARD's ADVANCE setting; SHIFT + the keys does the other). Stopped, the held keys also
  sound, so you hear the region you are writing. Slice tracks: the key's slice goes on the step.
- **RANGE page (B4) with a target**: the keys write the step's **range** lock the same way (the 16 keys span the
  whole sample), and R-ST / R-EN fine-tune it.
- **With a target, the knobs lock onto it** (as holding that step does); the cells show its locks.
- The screen shows a **step strip**: the step page's 16 steps, each with its trig and a small bar of its region lock,
  the target outlined; the waveform marks the target step's region.
- Built in 0.6.6 by hand: ec_loopstep.c (B11, the entries), ec_app_target_step (the target as a held step: knobs,
  cells, F1 + knob), the step strip and the target's lock on the waveform (ec_screens.c loop_steps / target_on_wave).
  **The zoom moved to B11 held + K1** (SHIFT + B11 is FILL); while B11 is held the white keys are labelled STEP n and
  the header says STEP  K1 ZOOM. Tests: test_loopstep (22 checks), test_ux's zoom; the real shot Loop_target.

**SHIFT fixes** (owner, 2026-11-03; built in 0.6.6 by hand)
- SHIFT + SEL is BPM in **0.1 steps**, accelerated (it was going through the snap path: tens). SHIFT + ALGO swing,
  one a click. BPM anywhere moves in 0.1 steps (the MASTER page too).
- SHIFT's screen covers only y 0..75 (BPM, swing, the track keys): **the page's own cells stay** below, and SHIFT + a
  value knob snaps that param (ec_snap.c, the norns snap lists). SHIFT's own BPM / SWING / TRACK / SNAP cells are
  gone.
- **SHIFT + B4 = RECALL, SHIFT + B5 = MEMORIZE** (B3 is dark now).
- 0.6.7 (owner: "B11 still does zoom"): on the web panel a mouse can't hold B11 and press a key, so **B11 is a
  tap toggle**: a tap turns pick mode on (B11 blinks, the header says PICK STEP, the white keys STEP n), a white key
  picks the target and ends it, a second B11 tap is LIVE. Held + a key still works; held + K1 is still the zoom, no
  longer named in the header.

**LOOP ENGINE waveform: the range, and the target's brackets** (owner, 2026-11-03; 0.6.8, by hand)
- LOOP / MACHINE / WARP show only range start..end (the target's range lock, else the track's, inside the trim),
  peaks computed from the sample for that window (cached); RANGE shows the whole sample with the range bracketed.
  Grains, slices and playheads are mapped into the window.
- With a target step the brackets are its region (its lock in the LOCK colour, else the track's), as norns did when
  a step was held; the orange bar under the waveform is gone.

**LOOP ENGINE step entry, second pass** (owner, 2026-11-03; 0.6.9, by hand; replaces the target step / ADVANCE and
the B11 pick mode above)
- B11 switches the white keys between the LOOP and STEP layers: held = the other layer while held; a quick click
  (nothing done while down) latches the other layer. B11 never goes LIVE; ALGO below step 1 does (ALGO selects one
  step).
- STEP layer: the keys select steps, several at once (the first key since the layer came up replaces the
  selection; later ones toggle). The selection stays until the next selection.
- LOOP layer: the loop keys are always live (scrub and sound); with steps selected each entry is also written into
  every selected step (region, or range on the RANGE page; a trig). Nothing advances.
- B11 held + OCT- clears the selected steps (trig and locks, one undo a page). The knobs lock onto all selected steps.
- Tests: test_loopstep (28 checks).
- 0.6.10 (owner: "on a step the loop keys need to work exactly the same as live"): the LOOP layer always draws and
  lights as live (the loop-key ruler, the key LEDs, the brackets while a loop key is held); only with no key held do
  the brackets show the first selected step's region. The step strip and step LEDs belong to the STEP layer.
- 0.6.10 (owner): step selection works as FIZZ: keys held together are one selection; once every step key is let
  go, the next press starts a new selection (that step alone). No toggling.

**0.6.11: the QA pass's five high findings** (results/qa-ux-0.6.10/report.html; the owner: "fix the five", 2026-11-03)
- The step selection and the target step are **each track's own** (ec_loopstep.c `ec_lp_track`, called from
  `ec_pn_select_target`): switching to T4 never writes into T1's selected step, and KEYBOARD's target step on T4 is
  T4's. The new track's target brings its step page back.
- On the **LOOP layer the selection is visible**: the step page's 16 cells as caps over the loop-key ruler (y 61..62;
  a selected step bright, the playhead the accent), so what the loop keys write into is in view without B11.
- **HOME from another mode is always LOOP ENGINE**, **FILE always BROWSE** (ec_panel.c mode_tap ignores `last` for
  them; the button labels say what will open). Tapping again still cycles (TRIG, KEYBOARD; the SAMPLER). EDIT keeps
  its memory (MIX / PAGES).
- **FILE opens while playing** (BROWSE, POOL, the SAMPLER, loads and previews). Only the EDITOR and RAZOR pages,
  FREE (F2, POOL's too), MOVE and purge say STOP FIRST. PLAY inside FILE works; from the EDITOR / RAZOR it goes to
  BROWSE first ("BROWSE: PLAY AGAIN"). The SAMPLER redirect and the flashing FILE button are gone.
- Tests: test_file's gating section rewritten, test_ux / test_v61ux adjusted, the demo scripts' HOME / FILE presses
  follow the new rule (one extra SAVE tap where the SAMPLER was wanted).

**0.6.12: LOOP ENGINE second QA round, and F1 / F2 in view** (owner, 2026-11-03)
- Stopped, the loop keys sound and now the playhead animates (the frame redraws while the track's reader or voices
  run, ec_app_frame).
- A loop entry's span never shrinks: with two keys held, letting go of one keeps the region (the DSP's keys_span);
  a further key while one is held widens the span; letting go of all ends the entry (return mode as before); the
  next press starts a new one.
- Playing: the brackets are the track's region (the resolved LOOP START / END: static markers, as norns); the bright
  span is what the engine plays now (a scrub, a held entry). The view window follows the resolved range (a step's
  range lock) while playing, not only the target's.
- LIVE (nothing selected) on a loop track: a loop entry sets the track's LOOP START / END in the keys' division
  (RANGE page: the range), one undo ("LOOP 3-7"), so the keys set the range and the knobs are no longer needed.
- LEDs: the STEP layer lights trigs mid, empty steps off, the selected steps blink. The LOOP layer lights only the
  active (resolved) range, the held keys bright, the playhead's key mid.
- F1 / F2: the header line names them at once ("F1 COPY/CLR LOCK", "F2 CUT PATTERN"), held or sticky; while sticky
  the header's right box shows F1 / F2 on the accent whatever the line says (a message, a held key).
- The step strip shows on both layers (owner): white on the STEP layer; on the LOOP layer in the layer's colour, red
  (ALERT) with steps selected, blue (the theme's SRC blue) LIVE, a rule under it in that colour; the loop-key ruler
  is gone from the firmware's screen (the brackets and the key LEDs show the keys' region). The 0.6.11 caps are gone.
- Tests: test_loopstep (the LIVE write), test_v61ux's strings.

**0.6.13: the pages on B11 + the black keys, as FIZZ** (owner, 2026-11-03: "changing pages is a chore through F3")
- LOOP ENGINE, **B11 held: B1..B10 are the pattern's pages** (the LEDs: the shown page lit, pages in the loop dim,
  muted faint; the labels PAGE n). The same gestures on PAGES' black keys (ec_mix.c, one page-key model):
  - tap = select the page (the step page shown);
  - hold (400 ms) = **mute** the page (out of the loop; hold again: back in; "PAGE 3 MUTED" / "PAGE 3 ON");
  - double-tap = **solo** (the loop is that page; double-tap again: ALL PAGES);
  - two keys held together = **loop between them** ("LOOP PAGES 1-2").
  F1 / F2 + a page key is still the clip (copy / cut / paste a page). B11's quick-click latch is not triggered by a
  page key. F3's white keys still work as before.
- Tests: test_loopstep test_pages_b11.

**0.6.14: ALGO on the page, PRESETS the page, 8 pages, B11 + OCT+ = LIVE** (owner, 2026-11-03)
- LOOP ENGINE's ALGO selects only the shown page's steps (1..16 of it) and never turns the page; full left is LIVE.
- PRESETS turns the step page in LOOP ENGINE and TRIG (no step held); the pool slot is the SLOT param (a held step +
  PRESETS still locks a slot).
- LEN tops out at 128 steps = 8 pages, so every page is on B11 + B1..B8.
- B11 held + OCT+ = LIVE (nothing selected); OCT- still clears the selected steps.
- The tempo always shows one decimal (120.0) in the SHIFT layer's cell and on F3 (owner). GLO LENGTH tops out at 128 too.
- The BPM knob's acceleration is capped: 0.1 a slow detent, at most 1.0 BPM a detent flat out (it reached 17.5).
- The LOOP layer's strip is all in its tint (owner): trigs, empty floors, the selection (a full column) and the
  playhead; the rule under it is gone (it meant nothing). The STEP layer stays white with the accent playhead.
- LOOP layer LEDs (owner, again): the active range's keys fully lit, the rest off (the playhead a dim trace outside
  the range), the selected steps blinking on their keys.
- GRAIN (owner: "the grains sweep across the whole loop and don't follow my sequence", "GSPD does nothing"): the
  spread was scaled by the file's length (SPRD 60 ms on a long file = a slab of the whole file, so the cursor's
  movement and the region were invisible); it is +-SPRD seconds around the cursor now. The grain view draws no frame
  box any more: the live region bright, the brackets and the playhead over the grains as on the other pages.
- Held-key texts (owner: "SELECT STEPS flashes on a quick B11 click"): a held key's header text shows only past the
  quick-click time, 300 ms (EC_HDR_MS, was 150), everywhere; B11's SELECT STEPS too (latched: at once).
- Colours (owner): the STEP layer's strip is red; the LOOP layer's is white with steps selected, blue LIVE.
- LOOP layer LEDs with a step selected (owner): the lit range is that step's region (its lock, else the track's), so
  stepping with ALGO shows each step's loop on the keys; a held loop key shows live.
- Colours again (owner): blue = LIVE, red = loop keys (steps selected), yellow (SPECIAL) = the STEP layer; the
  selected steps are white on every layer. The bars under the steps are their region (RANGE page: range) locks.
- B11 held (owner): the cells area is the **pages panel** (8 tiles: the shown page filled, in the loop outlined,
  muted dim, past the length a dot; LEN and RATE under them); **K1 = LEN, K2 = RATE** (the zoom moves to K3); two
  page keys held past the end grow the track to that page. Page LEDs: a muted page dark, the shown page muted blinks.
- The step model in all three step modes (owner): LOOP ENGINE, TRIG and KEYBOARD share B11 (held = the STEP
  layer / the pages on B1..B8 / K1 LEN, K2 RATE, K3 zoom; a quick click latches the layer, on a KEYBOARD slice track
  it is the bank), ALGO = the page's step (full left LIVE), PRESETS = the page, B11 + OCT+ = LIVE, B11 + OCT- = clear.
  The knobs lock onto the selected steps in TRIG too. TRIG's strip: the selected steps' white caps, a VELOCITY lock
  as the trig's height, a PITCH lock as a blue mark (above the middle: up). KEYBOARD: whole keys filled in the mode's
  colour (blue LIVE, red with steps selected, yellow on the STEP layer), the selected steps' notes white, its strip
  as LOOP ENGINE's, the pages panel while B11 is held.
- TRIG, a step held (owner): the black keys set its PITCH lock, B6 (F#) = 0, B1 -5 .. B11 +5 (the LEDs: the lock's
  key lit, B6 mid; the labels -5 .. +5). One undo; the trig stays on.
- "Most pages" (owner): the B11 layer (held: the STEP layer, the pages on B1..B8, K1 LEN, K2 RATE, K3 zoom, the
  pages panel) and ALGO = the page's step, PRESETS = the page, on every track page (FILTER, AMP, FX, MOD too) and in
  KEYBOARD; the knobs lock onto the selected steps there as well. A quick click on B11 keeps those pages' own tap
  (FILTER's envelope mode, FX MIX, MACROS). PAGES mode: K1 LEN, K2 RATE, PRESETS the page, without B11.
- **Design rule (owner): only a parameter assigned to a knob ever appears in a cell box.** PAGES' loop (set by the
  page keys) left the cells for the header's right info ("A1 LOOP ALL", "A1 LOOP 1-2", "A1 LOOP 3 PGS"); K3 is empty.
- MIX (owner): PRESETS selects the track (T1..T6, S1, S2, MST), as SHIFT + PRESETS does everywhere; the pool walk
  stays on the other modes (CROSSFADER, PATTERN LAUNCH, FILE).
- GSPD steps by 0.01 (owner: "super slow down the grains"; 0.05 was the floor above a freeze). Measured in the
  harness (ppos in ui_info): 1.0 = 125/1000 of the sample a half second, 0 = frozen, 4.0 = four times. Note: with a
  step selected the knob locks GSPD onto that step, not the track (the cell fills LOCK); go LIVE to set the track's.
- MIX (owner): PRESETS selects the track without a pop-up; the MIX page stays when the track changes (only MST's
  MASTER page comes and goes). Switching tracks no longer moves the step page to the new track's selected step.

**0.6.15** (owner, 2026-11-04)
- GRAIN DIR reads FWD, REV or "F70 R30" (the share of grains reading forwards); each drawn grain carries a blue head
  at its leading end (left = reverse). GSPD measured in the harness (ui_info ppos / gmean): the cloud follows the
  cursor at 0.14 as at 1.0 and 4.0, so the engine responds; a new version number every build so the web player
  never serves a stale cart under the old name.
- GSPD (owner: "each grain too"): besides the cursor's drift, GSPD scales each grain's own read rate (x PITCH;
  0.05 at the floor so a grain never stalls): at 0.1 every grain crawls, an octave and more down; at 4 four times up.
- FILE (owner: "once in the trim page the black keys are the zoom; make the browser and the editors separate modes
  on the SAVE button"): the FILE button cycles BROWSE / POOL > EDITOR > RAZOR > SAMPLER > BROWSE (the button's label
  says which; playing, the editors are skipped). EDITOR and RAZOR have no page keys any more (B4 / B5 are free in
  the browser); SEL still walks BROWSE < EDITOR < RAZOR < POOL. The layout (design/layout/elasticat.json) carries it.
- **Settings only on SHIFT + the button** (owner): a hold never opens them. SHIFT + HOME = LOOP SETUP (it was APP
  SETTINGS), SHIFT + ARP = APP SETTINGS, SHIFT + FILE = SAVE, SHIFT + SEQ = F3; the recorder's setup is REC held in the
  SAMPLER.
- **A held mode button is a mode picker**: past the quick click (300 ms) only its modes' black keys light (B1.. in
  order: HOME = LOOP, TRIG, KEYS; EDIT = MIX, PAGES; FILE = BROWSE, EDITOR, RAZOR, SAMPLER; the others their one),
  the labels name them, the header says PICK A MODE; a black key opens that mode and makes it the button's own;
  letting go without a key changes nothing. A quick click still cycles the modes.
- **MODE LOCK** (APP SETTINGS, off by default, saved in the settings record): a click opens the button's own mode
  and never cycles; the only way to change a button's mode is the hold + a black key.
- While a mode button is held the white keys are swallowed (nothing reaches the mode) and their LEDs are dark:
  only the picker's black keys light. APP SETTINGS is SHIFT + ARP (its SHIFT label says so); SHIFT + HOME is LOOP SETUP.
- **The EDITOR** (owner): K1 START, K2 END (the last one turned is the marker the white keys place, and the view
  scrolls to keep it in sight), K4 ZOOM round that marker; page 2 holds BPM, STEPS, GAIN; B1 / B2 the pages, B9 AUTO
  BPM (the trim as a power-of-two number of bars at the tempo nearest the project's). The scroll / zoom keys are gone.
- **SLICE** (was RAZOR; the header says FILE/SLICE): quieter: the other slices' edges dim, only the selected
  slice's number, the transient ticks only for 1.5 s after SENS is turned, no NOT RAZOR text; its black keys are its
  own (no page keys lead out: the FILE button does).
- **BROWSE / POOL** (owner): ALGO adds the file to the pool clockwise and removes it anticlockwise (no step page
  here); PRESETS turns the audible preview on / off (APP SETTINGS' AUTO-PREVIEW, saved); a scroll plays the file
  once. The white keys are loop keys over the previewed file (16ths, a held span loops, silent on release). B10 ADD,
  B9 REMOVE, B7 BROWSE, B6 POOL, B11 preview (BROWSE). POOL: B11 held takes the slot in hand; SEL or K1 move it, the
  list shows it riding on the cursor; letting go drops it (one undo); F1 no longer moves. The browser's SLOT /
  LENGTH / BPM / FREE are plain rows now, not cell boxes (no knob edits them: the design rule).
- EDITOR, second round (owner): K4 ZOOM on both pages; ALGO moves the selected marker; PRESETS changes the track's
  slot with the pop-up drawn as the POOL list (seven rows round the slot, no big text or bar); B11 = ZOOM 1X; B9
  AUTO BPM takes the file name's tag first ("bpm136") and otherwise fits a whole number of bars nearest the project's
  tempo; BPM and the trim no longer recalculate STEPS, B10 RECALC does. SLICE: solid 2-unit edges on the lattice.
  POOL: a slot in hand shows the result (the swap applied) with the two moving rows nudging. BROWSE / POOL preview
  keys: the span's LEDs lit, the playhead's key flashing inside it.
- SLICE, second round (owner): SEL stays on the editor's own pages (the EDITOR's too: the FILE button leaves);
  the other slices' edges sit behind the waveform in L3, the selected one in front; K1 SLCE START, K2 SLCE END, K3
  PLAY, K4 ZOOM (ZOOM on K4 of all three pages); page 2 SNAP / CHOKE / PITCH; page 3 COUNT / TRANS SENS (the slice
  functions' settings); B1..B3 the pages, B9 EVEN (COUNT even slices), B10 CHOP (at the transients), B11 the
  selected slice heard; a white key fires the slice on any machine (a slice machine: the pad, else the preview
  player). Both editors: B11 previews (the trim / the slice); the zoom reset is gone; B6 / B7 do nothing there.
- Both editors' B11 (owner): the trim / the slice loops while B11 is held and stops the moment it is let go.

## 12. 0.6.16: the QA session's fixes (2026-11-04)

Two harness sweeps over every mode (results/qa-0.6.15/REPORT.md). Fixed:

- **The slice table is on the undo stack** (EC_U_RAZOR): EVEN, CHOP, split, delete, mute, the slice knobs (start, end,
  play, choke, pitch, snap, sens: a knob's turns coalesce) and the COUNT knob each make an entry holding the table
  and the COUNT, so undoing COUNT brings the edges back with it (before: nothing of the table was undoable, and
  undoing CNT left stale, overlapping edges).
- **Auto-Chop starts at 0**: a hit in the first analysis window is found (the scan began at window 1), and when the
  first transient is late the head becomes slice 1 (before: the first hit was left out or split). Two onsets closer
  than 50 ms are one hit (was 30 ms: a hit's attack could count twice).
- **The machine changes only while stopped**, like the warp (the LOOP SETUP row says STOP, the knob STOP FIRST).
- **A held menu button let go with no black key changes nothing** (the release still clicked: the mode cycled).
- **MODE LOCK with FILE locked to an editor opens BROWSE while playing** (STOP FIRST), as the unlocked cycle does.
- **PAGES says LAST PAGE** when a hold cannot mute the loop's only page (it said PAGE 1 MUTED).
- **MOD CLEARED (F2 + a knob) is undoable** (the route's SRC / DST / DEPTH as one entry).
- **The trim editor's undo**: START, END, AUTO BPM and RECALC are gestures of their own (K1 then K2 no longer merge
  into one UNDO TRIM; undoing a trim no longer discards the steps RECALC set); the entry carries the slot's BPM too.
- Docs: FM1-CONTROLS.md's stale 0.6.10 lines (SHIFT + HOME, holds, the SLICE section, PATTERN's knobs, POOL,
  the recorder's setup), the guide's crossfader capture wording.

Kept as designed (the report called them out): KEYBOARD's black keys with a target step write the slice trig and
advance (the E1 demo writes slice sequences that way: a note key is not needed); the recorder's setup targets the
selected track's slot (E6: a resample; its SLOT row picks another); lowering COUNT keeps the table's edges (EVEN
redistributes); CROSSFADER capture takes the values that differ from their default or that the other scene holds.

## 13. 0.6.17: the loop brackets (owner, 2026-11-04)

- The waveform's brackets follow the loop keys while they are held (they moved only on release, when the entry
  became the track's region); a selected step's or the track's region shows as before when no key is held.
- The bracket glyph is a whole square bracket: a corner at the bottom as well as the top.

## 14. 0.6.18: the slice view on the track pages (owner, 2026-11-04)

- On slice tracks the LOOP ENGINE / TRIG waveform draws the slices as the SLICE editor does: the edges dark behind
  the wave (2 units), a sounding slice (a pad or a voice) a cream block with the wave cut out and its edges in front,
  muted slices dim, the playhead over it (before: dotted edges over a bright wave, a sounding slice a dark block).

## 15. 0.6.19: SLICE/SLICE (owner, 2026-11-04)

- The LOOP ENGINE pages are named after the track's machine: the header's tag and the first page's title are the
  machine's name (LOOP/LOOP, TRIG/TRIG, SLICE/SLICE, RAZOR/MACHINE, S.POLY/WARP, R.POLY/RANGE, NEIGH/.., INPUT/..)
  and so is B1's label, so the pages match the machine.

## 16. 0.6.20: the RANGE page on slice tracks (owner, 2026-11-04)

- Slice tracks draw the RANGE page as a loop track does: the whole file, the range bracketed (the lock colour for a
  target step's range lock), dim outside it, with the slices' edges dark inside the range (0.6.18's slice view had
  taken the page over and drew only the range).

## 17. 0.6.21 / 0.6.22: the loop keys' span, the RANGE page on every machine (owner, 2026-11-04)

- The loop keys' span: two keys held loop between them; let one go and the loop is kept while the other is held;
  press a key and the span refits to the keys held now, smaller or larger (0.6.11's "never shrinks" is gone). The
  region written on the last release is the last span. 0.6.22: the loop keys' LEDs light the live span while keys
  are held (they lit the track's region, which changes only on release).
- The RANGE page sets the range the same way on every machine (slice tracks included: their keys are not pads
  there), and its LEDs show the range over the 16 keys, not the loop keys.
- Slice tracks' RANGE page shows no slice edges: the same view as a loop track's.

## 18. 0.6.23: the loop keys' neighbour toggle, live values, the RANGE page's keys (owner, 2026-11-04)

- Pressing a held key's direct neighbour again, when the span is already just those keys, takes the span back to
  the held keys alone: W1 held, W2 = 1-2, W2 again = 1, again = 1-2. Only a direct neighbour does this.
- LIVE, the loop keys write the region (RANGE page: the range) at every press, so the cells, the markers and the
  LEDs follow at once; the header's message comes on the last release as before.
- The RANGE page's white keys are loop keys on every machine, editing R-START / R-END: the same span rules, LEDs
  and markers; they neither scrub the loop nor play pads there.

## 19. 0.6.24: the entry follows every press too (owner, 2026-11-04)

- With steps selected the loop keys behave exactly as LIVE: every press writes the entry (the region or range lock
  and the trig) into the selected steps, so their cells, the markers and the LEDs follow at once; the last release
  writes it once more with the header's message and closes the undo gesture.

## 20. 0.6.25: the loop keys' LEDs follow what plays (owner, 2026-11-04)

- LOOP page, LIVE: the loop keys' LEDs light what plays now (each step's region as it fires, the held keys' span),
  the DSP's playing region. RANGE page, LIVE: the resolved range (the playing step's lock).
- A selected step: both pages light its lock (the region or the range), keys held or not, since the entry is
  written into it at every press; the selected steps still blink.

## 21. 0.6.26: a selected step's markers while keys are held (owner, 2026-11-04)

- LOOP page, a step selected: the brackets and the bright span show the step's lock while the loop keys are held
  (the entry is written into it at every press), drawn in the step's own range window. They showed the engine's
  live span, which plays in the sounding step's range, so with per-step ranges they only matched after the release.

## 22. 0.6.27: no brackets while playing LIVE with no key held (owner, 2026-11-04)

- LOOP page, LIVE, the transport running, no loop key held: the start / end brackets are hidden; the bright span
  still shows what plays. They appear while loop keys are held, and as before when stopped, with a step selected,
  and on the RANGE page.

## 23. 0.6.28: the RANGE page on slice tracks = the loop track's (owner, 2026-11-04)

- Slice tracks draw the RANGE page through the loop track's code, so it behaves exactly the same: a selected step's
  lock, the keys' span while held, the playhead. LIVE, the range shown is the resolved one on every machine, so the
  playing step's range lock highlights as the pattern runs.

## 24. 0.6.29: the slices drawn across the range (owner, 2026-11-04)

- The chain is: the raw sample, trimmed (START / END); the range (R-START / R-END, per track, p-lockable) crops the
  trimmed audio (a crop, not a repitch); the chopper (the loop region, the COUNT grid slices, the RAZOR table) works
  only on the audio inside the range, in 0..128 of the range. The engine already mapped every chop that way
  (ec_track_map: trim, then range); the slice view drew the slices across the trimmed file while the view was the
  range window, so a range lock left only part of them visible. The slices now always span the view's range
  (the target step's lock, else the resolved range), all COUNT of them.

## 25. 0.6.30: a selected step auditions in its own range (owner, 2026-11-04)

- Stopped, with a step selected, the pads and the loop keys play through that step's region / range locks (the
  sequencer resolves the region params from the selected step: ec_seq_audition_step), so previewing slice 1 on a
  step whose range is locked plays slice 1 of that range, as the screen shows. Playing, the resolved values are
  the sequencer's as before (the locks apply as the steps fire).

## 26. 0.6.31: the last project survives a firmware update (owner, 2026-11-04)

- The last project's name is also kept in the folder, /data/last.txt (data/elasticat/last.txt in the RemoteFM1
  folder), written at every save and load. Power-on reads the flash record first; a fresh flash (a new firmware
  version, another device, a reinstall) falls back to the file, so the last project comes back instead of a new one.

## 27. 0.6.32: SLOT OFF, and slice voices that end (owner, 2026-11-05)

- **SLOT has OFF** (0), the default: a track plays nothing until its slot is set. A new project's T1 starts on slot
  1 (power-on still loads the folder's first file there); every other track is OFF. Nothing but the user changes a
  track's SLOT: K2 on the LOOP page, PRESETS, POOL's ASSIGN. BROWSE's ADD only fills the pool (it set the selected
  track's slot before, and every track started on its own slot, T1..T6 on 1..6, so adding a sample to the pool
  started another track playing). The cells read OFF, the sample area says SLOT OFF.
- **A slice voice ends**: with the amp release at its default (INF, for loops) a slice kept ringing for the 2 s
  release clamp after a 1SHOT reached its end or a HOLD let go; INF now means a 20 ms release for slice voices.

## 28. Docs: FM1-CONTROLS.md caught up to 0.6.32 (2026-11-05)

- No code change (the cart stays 0.6.32). FM1-CONTROLS.md now describes the loop keys' span rules (press refits,
  release keeps, the neighbour toggle, live writes, LEDs following what plays, brackets hidden while playing LIVE),
  the RANGE page as loop keys on every machine, the range as a crop of the trimmed, stretched sample, the slice view,
  a selected step's audition, SLOT OFF, LOAD filling the pool only, and last.txt. The parameter reference's SLOT,
  R-START / R-END, amp release and trim start texts (design/layout/descriptions.json) say the same.

## 29. 0.6.33: SLOT on K2 on every machine; SHIFT reaches OFF (owner, 2026-11-05)

- The LOOP page on slice machines (SLICE, RAZOR) is P/T · SLOT · SLIC · PLAY (SLOT and PLAY swapped), so SLOT is
  K2 on the first LOOP page whatever the track's machine.
- SHIFT does not snap SLOT: with SHIFT held the SLOT knob steps one slot a click, as without it, OFF included (it
  snapped through 0.5's 1, 2, 4, 8, 16, so SHIFT could never reach 0.6.32's OFF).

## 30. 0.6.34: no TRACK pop-up from SHIFT + PRESETS (owner, 2026-11-05)

- Changing track with SHIFT + PRESETS no longer shows the big TRACK takeover: it was hidden under SHIFT's screen and
  flashed up for the rest of its second when SHIFT closed. The SHIFT screen's track keys and the header show the
  track.

## 31. 0.6.35: old projects keep their slot-1 tracks (2026-11-05)

- A project file leaves out the params that equal their default. SLOT's default was 1 until 0.6.32 (OFF since), so
  a project saved before 0.6.32 left out the SLOT of every track on slot 1, and 0.6.32-0.6.34 loaded those tracks as
  OFF (silent, their keys previewing nothing). The project record is now version 3 (the same layout); in a version 2
  record a missing SLOT is slot 1 again. A project 0.6.32-0.6.34 saved with a track OFF brings that track back on
  slot 1 (set it OFF again). test_proj checks both versions.

## 32. 0.6.36: a save never overwrites a file another tab wrote (owner, 2026-11-05)

- Two player tabs on the same folder (every RemoteFM1 web tab on one address shares it, and the FM-1 drives each
  connected tab) both save the current project; a stale tab's autosave overwrote the newer SHIFT + SAVE.
- Each instance keeps a checksum of the project file as it last loaded or saved it. Before every save (auto or SHIFT
  + SAVE) it reads the file back; if it differs, nothing is written and the header says **NOT SAVED: FILE CHANGED**.
  Autosaves keep refusing; the **next SHIFT + SAVE writes this instance's project as a new one** (the next free
  name), so both versions are kept. A missing or unreadable file is written as before. test_proj checks it.

## 33. 0.6.37: slice pads' LEDs and a selected step's slices (owner, 2026-11-05)

- On slice tracks' LOOP ENGINE pages the white keys' LEDs are all off, except: the pads held; LIVE, the slices
  sounding; with a step selected (ALGO, or the selected steps' target), the slices that step plays (its slice trigs,
  else a trig's SLICE lock or the track's SLICE). Muted pads no longer show faint, nor do idle pads dim.
- The waveform does the same: with a step selected its slices are the cream blocks (as a sounding slice is LIVE), so
  scrolling ALGO through the steps shows each step's slices; LIVE it shows what sounds, as before.

## 34. 0.6.38: a loop-key gesture is one undo (2026-11-05)

- Since 0.6.23 the loop keys write at every press. Each press and the last release each made an undo entry of their
  own (LIVE: the params entry never coalesced; with steps selected only within 1 s), so the last entry held the
  values the presses had already written and SHIFT + F1 said UNDO LOOP but changed nothing. Now an entry's undo is
  recorded once, at its first press, before anything is written, and the gesture closes on the last release: one
  undo brings back the region / range (or the selected steps) as they were before the keys went down.
  test_loopstep checks it (a key held past the coalescing window; a two-key span pressed slowly).

## 35. 0.6.39: slice keys dim, COUNT is never a slice's lock (owner, 2026-11-05)

- Slice tracks' LOOP ENGINE keys: every key with a slice (1..COUNT) is dim, so the count shows; held / sounding (LIVE)
  / the selected step's slices bright; the keys past COUNT dark.
- A pad held + a knob locks that param on the slice (its program). COUNT, SLICE, SLOT, LENGTH, LDIV and RATE are the
  track's structure: with a pad held they now edit the track as usual (COUNT turned with a pad held made a slice lock
  that F1 + K1 could not find: NO LOCK).
- F1 + a knob clears slice locks too: with pads held, those slices' lock of that param; with nothing held, every
  step's and every slice's. test_loopstep checks it.

## 36. 0.6.40: the step page in the header, B11 + PRESETS / ALGO (owner, 2026-11-05)

- The header has a second dark box beside the track's (T1 | S1): the step page PRESETS turns (S1 = steps 1-16, S2 =
  17-32 ..), on every page where PRESETS turns the step page (the track pages and KEYBOARD). The title follows it.
- B11 held + PRESETS = the zoom (as K3); on slice tracks the zoom is the pads' bank: right = SLICES 17-32, left =
  SLICES 1-16, one click, and it no longer writes the track's LOOP DIV (the loop keys' division).
- B11 held + ALGO = the pattern length, as K1.

## 37. 0.6.41: TRIG on LOOP ENGINE's B1, the key LEDs, the bank bar (owner, 2026-11-05)

- LOOP ENGINE's B1 is TRIG: one page with four lanes (a tap steps them, the header's right box says P1-P4): NOTE,
  LENGTH, VELOCITY, MICRO; SLIDE and the ENV / LFO / FILTER resets; CHANCE, CONDITION, RATCHET; the playhead (SNAP
  START, RET MODE; slice machines: the slice, its play mode, ratchet, reverse). The waveform and the loop keys stay
  as on LOOP. The pages moved one key on: B2 LOOP, B3 MACHINE, B4 WARP, B5 RANGE, and their hold menus with them
  (B1 holds ADVANCE). Power-on and a project load open the LOOP page (B2), as before.
- The source template's black keys read apart: the page keys ~40 %, the source (mod) keys ~10 %, the page shown
  full. Slice tracks' keys with a slice ~10 % (were ~16 %).
- B11 held on a slice track: a bar under the waveform spans the slices of the pads' bank (1-16 / 17-32, in the
  layer's colour), so B11 + PRESETS shows where the bank went.

## 38. 0.6.42: FILE split: SAMPLES and PROJECTS (owner, 2026-11-05; docs/PROJECTS-DESIGN.md)

- **SAMPLES** (the browser): rooted at AUDIO/, nothing above it; below it an **UP row** on top (an arrow, where it
  goes). **OCT+ opens** the row (UP: up, a folder: in, a file: LOAD), **OCT- goes up**; FREE is B9 (it was F2).
- **PROJECTS**, FILE's second mode (FILE: SAMPLES > PROJECTS > EDITOR > SLICE > SAMPLER; FILE held: B1-B5): every
  project in /data/projects, newest first (each file's time; the same second: the higher name first), the loaded one
  OPEN; the cursor's card (ec_project_peek: its name, its 64 patterns, tempo, samples, from the file's first 64 KB).
  **OCT+ / B10 LOAD** (asks with unsaved edits: LOAD X?, OCT+ yes / OCT- no), **OCT- back**, **B9 SAVE**, **B8 SAVE
  +1** (name + the next free number, now the project), **B1 NEW** (asks). B2 RENAME / B3 RANDOM come with the name
  keybed (0.6.43). The header asks with the back / OK arrows: OCT- / OCT+ are no / yes in the browsers.
- Tests: test_file (the root, UP, OCT), test_proj (LOAD through PROJECTS with the question, SAVE +1, NEW), the
  scripts' FILE taps one more (past PROJECTS).

## 39. 0.6.43: PROJECTS' keys, FILE/SAMPLES (owner, 2026-11-05)

- PROJECTS: the white keys do nothing (they edited the track's steps) and are dark. B11 does nothing there either
  (the owner saw it open the slice editor; not reproduced in the harness or the player: watching).
- The browser's header is FILE/SAMPLES (it said FILE/BROWSE).
- The name keybed, RENAME and RANDOM: 0.6.47 (section 43).

## 40. 0.6.44: SAMPLES' OCT+ and B11 toggle the pool, B11 held previews (owner, 2026-11-05)

- OCT+ on a file puts it in the pool or takes it out (out: as B9 FREE, refused while playing); on a folder or UP it
  opens, as before.
- B11: a quick click does the same as OCT+; held (300 ms), the file under the cursor previews until B11 is let go.
  With AUTO-PREVIEW off (PRESETS), hold B11 to hear a file and click it to take it: the fast way through a folder.
- The labels: B11 and OCT+ on a file +/- POOL. test_file checks hold, click and OCT+.

## 41. 0.6.45: PROJECTS: DELETE, OVERWRITE asked, SAVE +1 shown, by name (owner, 2026-11-05)

- B7 DELETE: asked (DELETE X?, GONE FOR GOOD in ALERT); yes deletes the file and its hidden copy. The open project
  deleted stays open, unsaved (EDITED); its next save makes a new file.
- B9 SAVE with another project under the cursor asks (OVERWRITE X?, THIS PROJECT REPLACES IT): yes writes this
  project over that file, which is the project from then on (SAVED OVER IT). On the open project: a plain save.
- B8 SAVE +1: the cursor goes to the copy once listed, the header says SAVED +1.
- The list is by name, naturally: the numbers by their value, a name before its numbered copies (ZEBRA, ZEBRA 2,
  ZEBRA 3, ZEBRA 10, ZEBRAS), case aside; no file times read any more. SEL / ALGO move by the whole turn.
- test_proj checks the order, DELETE and OVERWRITE.

## 42. 0.6.46: PROJECTS' keys as SAMPLES', loading stays, the cat (owner, 2026-11-05)

- Keys: B11 LOAD (as SAMPLES' B11; OCT+ too), B10 SAVE (another project under the cursor: OVERWRITE?), B9 SAVE AS
  (+1), B6 DELETE (asked), B1 NEW, B2 RENAME, B3 RANDOM.
- **The user drives mode changes**: a load (and NEW) stays in FILE/PROJECTS; it no longer jumps to LOOP ENGINE.
- Loading (and NEW): a pop-up over the card, the dancing cat (MIX's CAT sprite, alone) at 10 fps, at least 1.2 s
  and as long as the load lasts; the header LOADING, then NAME LOADED.

## 43. 0.6.47: FIZZ's name keybed, RENAME, RANDOM, files named after their projects (owner, 2026-11-05)

- Every name (a pattern's, the project's, a project file's) is typed on FIZZ's typewriter keybed: B1..B11 A..K,
  W1..W16 L..Z and SPACE, SHIFT the digits and file-safe signs; the suggested name selected (the first key replaces
  it); PRESETS / K4 right rolls (FIZZ's roller ported: namesizer pairs, phonic and cosmic words), left walks back
  through the last ten; ALGO the cursor, SEL / K1 the character; OCT- delete (empty: cancel), SHIFT + OCT- clear,
  OCT+ / FILE OK, HOME cancel. The keybed on screen (two rows, as the FM-1's), the typed key glowing, all keys lit.
- PROJECTS: B2 RENAME (the keybed on the project under the cursor; OK renames its file: the open project is saved
  under the new name and the old file deleted, another one copied and the old deleted), B3 RANDOM (renamed at once).
- Files are their projects' names: a project's first save (and NEW) takes its name, else a rolled one; a name another
  file has gets the next free number (only a trailing " N" counts as a number: PROJ-001 -> PROJ-001 2).
- Tests: test_proj's keybed (typing, OCT-, rename, roll back, RANDOM), names (rolled, a taken name numbered), the
  e5 demo types its pattern name (ELASTIC).

## 44. 0.6.48: one RENAME key, K2 / K4 roll, one font size, the rolling die (owner, 2026-11-06)

- PROJECTS: B2 RENAME only (B3 RANDOM gone: in the keybed K2 or K4, as PRESETS, roll names).
- The keybed's name is always the big font: 14 characters typed or rolled (the roller keeps only names that fit
  whole); an older, longer name scrolls with the cursor.
- The die is ACTION blue, under the name beside the count, and tumbles each roll (faces flipping, a hop) for 0.6 s
  before it lands on a face.

## 45. 0.6.49: F1 + SEL rolls the page's params (owner, 2026-11-06)

- F1 held, SEL turned on any page with params: two dice on a card (ACTION blue, the accent frame) shake, harder with
  every detent; at 10 they are thrown (they tumble across, 0.45 s) and as they land every param on the page gets a
  random value (options and switches too, integers whole), as one undo entry (EC_U_NV is 16 now: a page's four
  lanes). Kept: the machine, warp mode, slot, slice count / slice, length, rate, loop division, FX types, volume, mute
  and the stopped-only params. F1 let go before the throw cancels; the turns never make F1 sticky. test_ux checks it.

## 46. 0.6.50: the bars and the FILTER / ENV pictures show the modulation (owner, 2026-11-06)

- A param cell's bar shows the value as heard: the track value with the matrix's generic routes and the engine's
  fast buses (PITCH, CUTOFF, RES, AMP, PAN) applied, from the engine's live sources (ec_app_mod_values). A step's
  lock still shows as before; the knob's own value (the text) stays the value set.
- The FILTER curve and the ENV shapes follow the same modulated values. While a step is held they hold still: that
  step's locked value, or the track's value where it has no lock.
- With a live route on the track shown the screen redraws about 30 times a second. test_ux checks LFO 1 -> CUTOFF.

## 47. 0.6.51: modulation shown as set / heard, unipolar LFOs, 0.1 depths, ALGO on LOOP only, F1 on a held step, LFO routes (owner, 2026-11-06)

- The bars and the FILTER / ENV pictures move with the modulation only while playing; stopped they show the values
  set. A source key held (mapping a mod): the set value moved by that source's routes at their full amount (depth x
  the source's level), still. A step held: that step's value (its lock, else the track's), as before.
- The LFOs are unipolar (0..1 x their level): a positive depth only ever adds. The LFO page draws the wave rising
  from a bottom line, its meter from the bottom.
- A matrix DEPTH moves 0.1 a detent (the knob's speed-up makes the bigger moves; SHIFT still snaps 0 / 25 / 50 / 100)
  and always reads with one decimal: 1.0, -12.5, 100.0.
- ALGO selects a step (and LIVE) in LOOP (its pages, TRIG) and KEYBOARD only, as B11's STEP layer; on FILTER, AMP,
  FX, MOD... the white keys are the steps: hold one and turn a knob to lock. ALGO there turns the step page.
- A step held + F1: that step's lock of the param a knob turned last is cleared, when that param is on the page
  shown ("NOT ON THIS PAGE" otherwise). F1 held + a knob still clears every lock of that param.
- LFO 1 / LFO 2 / MOD ENV / VEL pages: the header shows one route at a time with its number (1/3); SEL steps through
  the source's routes (past the ends: the pages), ALGO (DEPTH) sets the shown route's depth.

## 48. 0.6.52: no route count, ALGO idle on the track pages (owner, 2026-11-06)

- The LFO / MOD ENV / VEL header shows the route without its number (the 2/2 is gone).
- On FILTER, AMP, FX, MOD (the track pages outside LOOP) ALGO does nothing (no label): PRESETS turns the step page.
  On the LFO / MOD ENV / VEL pages it stays the route's DEPTH.

## 49. 0.6.53: the QA pass's data-loss fixes (results/qa-0.6.52/REPORT.md A1-A5, 2026-11-06)

- A1: a save that fails or is refused (a write error, NOT SAVED: FILE CHANGED, TOO BIG) leaves the project EDITED:
  PROJECTS shows it, LOAD X? asks, and autosave tries again after the next edit (no retry loop).
- A2: a LOAD or NEW waiting on that save is cancelled: nothing replaces the unsaved project.
- A3: a name never starts with "." (the keybed drops leading dots and spaces, as do new file names): ".X" would be the
  hidden copy of X.
- A4: a record whose checksum is right but whose body doesn't parse is broken: the hidden copy is tried, else DAMAGED
  FILE, and the open project stays (it was replaced by an empty one, which the next autosave wrote over the file).
- A5: the recorder's setup on a track with SLOT OFF records into the next free slot (NO FREE SLOT when the pool is
  full), never over slot 1.
- test_proj checks each.

## 50. 0.6.54: the QA pass's quick fixes (results/qa-0.6.52/REPORT.md C1, C2, C4, C5, C7, 2026-11-06)

- C1: LOOP ENGINE's B3 hold menu (MACHINE) says STOP FIRST while playing, as the warp menu and LOOP SETUP do.
- C2: MIX: PRESETS walks the targets on from S1 / S2 (it stopped at S1).
- C4: the step page shown stays inside the track's length: a shorter LEN (PAGES, B11 + ALGO, an undo, a recall,
  another track) brings it back, so a white key never writes a trig past the end.
- C5: KEYBOARD: a note's advance moves the selected step with the target, so ALGO counts on from the step shown.
- C7: the dice land every number on its own step (PITCH whole semitones) and roll INSERT 2's P1..P8 in the FX param
  each stands for (its range, its options).
- test_ux (test_qa_quick) and test_fx2 check each.

## 51. 0.6.55: the undo pass (results/qa-0.6.52/REPORT.md B1-B10, owner 2026-11-06)

- B1 (owner): in the SLICE editor SHIFT + F1 / F2 are undo / redo, as everywhere (their labels say so); EVEN and
  AUTO-CHOP are B9 / B10 only.
- B2: F2 + a knob (no source key) clearing a param's routes is one undo entry.
- B3 / C9: a source key held + knobs: the routes it made and kept are one undo entry, recorded as the key is let go
  (with the source's level, when the hold raised it from silent); the depth detents of a route the hold made add no
  entries of their own. A hold that keeps no route puts the level back. A track change mid-hold: the release still
  works on the hold's track.
- B4: slice locks (a pad held + a knob, F1 + a knob clearing them) are undoable.
- B5: an entry into selected steps on several step pages, B11 + OCT- clear, and F1 + a knob clearing every lock of
  a param are one undo entry each (not one a page).
- B6: pattern CUT and PASTE are undoable (the pattern as it was, with its name, BPM, page loops, slice programs).
- B7: scene clear (SHIFT + B1 / B2), capture (B11 + B1 / B2) and a scene's knob edits (B1 / B2 held) are undoable;
  the crossfader stays where it is.
- B8: page loops (a hold, a solo, a span that grows the track, F3's held page) are undoable, the length with them.
- B9: live REC: one undo entry a pass and track (the track before its first recorded step), not one a step.
- B10: MIX's hold-mute and ALL TRACKS ON; FREE of a pool slot (undo loads its file again, its trim and BPM kept).
- Under it: a new entry kind keeps a big part (a pattern, the scenes, a track) in a ring of 8 snapshots; undo and
  redo swap it with the project. An entry whose snapshot the ring has reused says TOO OLD and changes nothing.
- The e6_demo test has its own folder (it raced test_e6 over e6_folder's recordings).
- test_file (test_undo_pass, test_free_purge) checks them.

## 52. 0.6.56: headroom, the owner's design answers, the QA's smaller bugs (2026-11-06)

- Headroom (owner): every source (the loop readers, the slice voices, the live input) enters its track's chain at
  -6 dB, so the filter, drive, inserts, sends and buses never clip inside; the output's soft limiter is linear to
  half scale, then a smooth knee that never reaches full scale (0.98). The QA's 6-track scenario: no sample at full
  scale (it clipped 0.7 % before), peak 0.974. Everything is 6 dB quieter: the demos' RMS floors are halved.
- No autosave while playing (as before: autosave runs only while stopped).
- The LFO / MOD ENV / VEL meter rests while stopped (as the pictures since 0.6.51).
- The dice (F1 + SEL) keep LFO 1's and LFO 2's levels (as the volume).
- COUNT 16 -> 10 -> 16 in the SLICE editor gives back the six slices it hid, where they were (the table was always
  kept; a test now holds it); EVEN respreads. Grid slice machines (SLICE, S.POLY: the LOOP pages' slice mode) always
  play COUNT even slices: the SLICE editor now shows that grid on them, and its table edits (START / END, split,
  delete, EVEN, AUTO-CHOP) say GRID SLICES instead of changing a table those tracks don't play (QA C16).
- QA C3: MIX with S1 / S2 / MST: no track cells (they showed T1's and did nothing).
- QA C6: PAGES: a span whose second key is B9 / B10 loops as the other order does; B11 with a key held does nothing
  (it muted the held key's page).
- QA C10: DELETE of the open project leaves it EDITED (autosave waits for an edit; it wrote the file straight back).
- QA C11: a project's record names its file: the first save (packed again once named), SAVE AS +1, OVERWRITE, and a
  load (the file's name wins over an older copy's).
- QA C12: a LOAD takes the first free slot no track or step lock points at (after a FREE the track stays silent).
- QA C14: REC does nothing in PROJECTS; in the SAMPLER a tap never arms live REC (held: the recorder's setup).
- QA C15: KEYBOARD: a full-velocity note clears the step's softer velocity lock.
- QA C17: labels: B11 held, ALGO LEN and PRESETS ZOOM; PAGES, ALGO / PRESETS PAGE and no K3 ZOOM; MIX / XFADE /
  PATTERN, ALGO nothing (no hidden step page), XFADE / PATTERN PRESETS nothing (it changed the track's sample; the
  slot walk is the EDITOR's PRESETS); a slice track's entry says SLICE 19 > 2; LOOP 3 > 2 fits the header.
- test_ux (test_056), test_file, test_proj check them.

## 53. 0.6.57: KEYBOARD live REC, its step strip and LEDs (owner, 2026-11-06)

- Live REC in KEYBOARD records each note on the step where it is played, with a target step selected or not (it
  wrote every note into the target step and advanced from there, so a 7-step pattern filled from its end).
- KEYBOARD draws LOOP ENGINE's step strip (the playhead moving, trigs, ghosts, locks, the selection, velocity and
  pitch marks) instead of its own thin row.
- KEYBOARD's keys show a step's note: the selected step's (ALGO) blinks on its key, as LOOP's selected step does;
  playing LIVE, the note of the step playing now lights. The black keys too.
- test_ux (test_kb_rec) checks the recording and the blink.

## 54. 0.6.58: the pads' bank in KEYBOARD, and only with more than 16 slices (owner, 2026-11-06)

- KEYBOARD, B11 held on a slice track: the bar under the slices of the pads' bank (1-16 / 17-32), as LOOP ENGINE's.
- The second bank (17-32, B11 + K3 / PRESETS) only with more than 16 slices; with 16 or fewer it stays 1-16 (it says
  SLICES 1-16), and COUNT lowered to 16 or fewer brings bank 1 back. test_ux (test_bank16) checks it.

## 55. 0.6.59: CAPTURE for quick takes, QUANTIZE, notes as long as played, ALGO's preview (owner, 2026-11-06)

- CAPTURE: REC arms it (stopped or playing; the header says REC: PLAY A KEY). The first key (a loop key, a pad or a
  KEYBOARD note) clears the track's pattern (every page back in the loop; one undo for the whole take) and starts
  playback from step 1 (restarting it when playing), and that key is step 1. The track's length grows with the
  playhead (one step ahead, so it never loops mid-take) up to 128 steps, where recording ends and the take loops.
  REC again: the length is where the playhead got to (the nearest step), and the take loops from step 1 at once
  (CAPTURED n); PLAY: the same length, playback stops. Press REC, play, REC: a new take each time.
- Every take keeps its micro timing (unless the REC setup's QUANTIZE is HARD) and each note's held length (a LENGTH
  lock, in quarter steps, 0.25 .. 16). A KEYBOARD note playing live ends as its key comes up (it held on), as a
  recorded one ends after its length.
- QUANTIZE (QUANT, K3 on TRIG's last lane and TRIG mode's PLAYHEAD page): 0 % as played .. 100 % on the grid, a morph
  of every step's micro timing. F1 + the knob (its label APPLY QUANT) applies it: the steps keep the timing it gives,
  the knob goes back to 0 % (one undo).
- Stopped, ALGO onto a step plays that step once for its length (a note at its pitch in its region, a slice step its
  slices); an empty step is silent.
- test_liverec (the CAPTURE take, CAPTURED 16, the loop, QUANT 100, APPLY) and test_ux (test_algo_preview) check them.

## 56. 0.6.60: KEYBOARD's LEDs, as the owner reads them (2026-11-06)

- The selected step's note: its key lit, steady (0.6.57 blinked it). The selected step itself blinks on its own white
  key (its place on the step page, as LOOP ENGINE's); a key that is both blinks lit / mid. Playing LIVE, the note of
  the step playing now stays lit.

## 57. 0.6.61: CAPTURE armed while playing (owner, 2026-11-06)

- REC in CAPTURE while the transport runs stays armed until the first key (the tick ended the wait at once while
  playing, so the first key recorded over the old take: the old notes went on playing). test_liverec
  (test_capture_over_old) checks it armed while playing and while stopped.

## 58. 0.6.62: live keys sound and override, black-key holds, KEYBOARD's pages and scales (owner, 2026-11-06)

- TRIG, playing: a loop key or a KEYBOARD note held sounds from its region's start (as a trig's gate) until it comes
  up; it was silent unless a step's gate was open, so a CAPTURE take on TRIG was silent while played.
- A live loop key or KEYBOARD note held overrides the sequence on its track (its trigs don't fire) until it comes up;
  slice tracks' pads as before.
- Black keys with a hold (LOOP ENGINE's B1..B5 menus, FILTER / AMP's B1 quick setting, FX's slot types): the page
  only on a quick click, on release; a hold that opened its menu or moved a knob leaves the page alone.
- KEYBOARD: SEL turns its knob pages: NOTES (OCTAVE / SLICE, PITCH, VEL, STEP), SCALE (SCALE, ROOT, LAYOUT, LEN),
  SOUND (CUTOFF, RESO, ATTACK, RELEASE).
- Scales (the track's, saved): CHROM, MAJOR, MINOR, DORIAN, PHRYG, LYDIAN, MIXO, LOCRIAN, HARMMIN, MELMIN, MAJPENT,
  MINPENT, BLUES, WHOLE, on any ROOT. CHROMATIC layout: the keybed as before, the root's keys mid, the scale's notes
  dim, the others faint. IN SCALE layout (loop tracks): the 16 white keys play the scale's notes up from the root
  (W1 the root at the octave shown), drawn as 16 keys with the roots marked; the black keys are shortcuts: B1..B3
  the knob pages, B6..B10 the mod sources (LFO 1, LFO 2, ENV, VEL, AT: held + a knob maps it), B11 as before. The
  header names the scale (A MINOR).
- test_ux (test_062) checks them.

## 59. 0.6.63: F1 / F2 copy / cut on LOOP ENGINE's keys (owner, 2026-11-06)

- LOOP ENGINE's white keys are the loop keys, so with F1 / F2 on (held or sticky) they never reached the steps:
  copy / cut / paste of steps did nothing there. With F1 / F2 on they are the steps now, as on every other step
  page. test_ux (test_clip_modes) copies and moves a step on FILTER, TRIG, LOOP ENGINE and AMP.

## 60. 0.6.64: KEYBOARD's STEP layer as FIZZ's (owner, 2026-11-06)

- B11 (held or latched) on a loop track: the white keys select steps (as before) and the black keys B1..B10 play the
  track's scale up from its root (grouped, whatever the layout; CHROM: semitones), at the octave shown; a note
  enters the selected step as before. Their LEDs: the root mid, the others dim, the selected step's note lit; their
  labels the notes. (The black keys played the chromatic sharps there.) test_ux (test_kb_step_layer) checks it.

## 61. 0.6.65: settings in the folder, KEYBOARD's scale on screen and LEDs (owner, 2026-11-06)

- The host keeps a firmware's flash per version, so every new build started with fresh APP SETTINGS (ADVANCE back
  on, the theme, knob speed ..). They live in the folder now (/data/settings.bin, written with every change) and
  power-on reads them from there first; the flash is only the fallback with no folder (and gets the folder's copy).
  The last project's name too: /data/last.txt first, the flash's name only when the folder has none; every save
  points it at the file just written (SAVE AS +1, OVERWRITE, RENAME too). The REC setup is the project's (saved with
  it: SHIFT + SAVE, or the autosave once stopped). test_proj (test_new_build) boots a new build on the same folder.
- KEYBOARD: the empty steps in the layer's colour, as LOOP ENGINE's (blue LIVE, red steps selected, yellow STEP).
  The on-screen keyboard shows the scale: its white keys bright, the others dim, its sharps grey, the others black,
  the roots marked (drawn by each key's place on the keybed, whatever the STEP layer's black keys play).
- KEYBOARD's LEDs: the notes out of the scale off, the root mid, the scale's other notes dim.
