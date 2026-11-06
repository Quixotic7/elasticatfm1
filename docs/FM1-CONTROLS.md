# Elasticat on the FM-1: controls (0.6.32)

The FM-1 is the panel; the sound runs in the RemoteFM1 host. Samples come from your `RemoteFM1/audio/` folder. Six
tracks **T1-T6**, each with its own sample slot, machine, warp mode, filter, amp, insert FX, sends, mod matrix and
step pages; two send buses **S1**, **S2** and the master insert **MST**; 64 patterns (4 banks of 16); a pool of 128
sample slots; two scenes **A / B** with a crossfader.

0.6.0 introduced **modes** and 0.6.1 kept them (no pop-ups, messages in the header, two inserts per track, projects as files). Each mode button opens a mode, every mode is a main view, and the keys change with the
mode. The design is docs/UX-V2.md; this file follows the code. Where the two differ, see "Differences from
docs/UX-V2.md" at the end.

Words used here:

- **White keys W1-W16** (F3..G5) and **black keys B1-B11** (F#3..F#5), as in FIZZ. Only F3..C5 have pressure (W1-W12,
  B1-B8).
- **F1** = the OCT- button, **F2** = the OCT+ button.
- **SEL** = the SELECT knob, **ALGO** = ALGORITHM, **PRESETS** = PRESETS, **K1-K4** = KNOB 1-4.
- **GLO is SHIFT.** FILE is the button printed SAVE.
- A **hold** is 0.4 s.

## The panel at a glance

| Button | Mode(s), in tap order | Tapped again | Hold 0.4 s | SHIFT + |
|---|---|---|---|---|
| FX | FX | the next FX group (INSERT 1, INSERT 2, SEND 1, SEND 2, SENDS / MASTER, MIX, then INSERT 1 again) | FX settings | FX settings |
| SEL | FILTER | the next page | filter settings | filter settings |
| ENV | AMP | the next page | amp settings | amp settings |
| LFO | MODULATION | the next page | (none: NO SETTINGS) | (none) |
| EDIT | MIX → PAGES | the other one | the project settings (NAME, GLO BPM, BPM, RETURN MODE) | the same |
| HOME | LOOP ENGINE → TRIG → KEYBOARD | the next one | the loop / machine settings, with KEYBOARD's ADVANCE | **LOOP SETUP** (APP SETTINGS is SHIFT + ARP) |
| FILE (SAVE) | FILE → SAMPLER | the other one | the recorder's setup | **save the project** |
| ARP | CROSSFADER | - | (none: NO SETTINGS) | (none) |
| SEQ | PATTERN LAUNCH | - | F3 | F3 |

- **Each mode remembers its page.** EDIT comes back to the sub-mode it was on (MIX / PAGES). 0.6.11: **HOME from
  another mode is always LOOP ENGINE** (tap again: TRIG, KEYBOARD) and **FILE is always BROWSE** (again: the SAMPLER),
  so a blind press lands in one place.
- 0.6.15: **settings open only on SHIFT + the button** (SHIFT + HOME = LOOP SETUP, SHIFT + ARP = APP SETTINGS; the
  recorder's setup = REC held in the SAMPLER). **A held button is a mode picker**: its modes on the black keys, PICK A
  MODE in the header; let go without a key = nothing (0.6.16: so, no click either). **MODE LOCK** (APP SETTINGS) stops clicks
  from cycling (0.6.16: FILE locked to an editor opens BROWSE while playing: the editors need a stop).
- **Settings stay open** when you let go. F1 or F2 closes them, back to the mode they were opened over. The mode's
  own button (or SHIFT + it again) goes back too. Another mode button closes them and opens its mode.
- **Power-on and project load: LOOP ENGINE.** There is no separate "main view" any more.
- **Entering a mode shows nothing extra** (0.6.1: the mode pop-up is gone): the mode's screen, its header.
- **The intro** (the logo animation) plays at power-on. Any key or knob skips it. APP SETTINGS > INTRO turns it off.

The other controls:

| Control | Does |
|---|---|
| White keys | the selected track's steps (most modes; see each mode) |
| Black keys | per mode |
| MASTER | master volume |
| SEL | the mode's page list, both ways, stopping at its ends (lists, the browser, the MATRIX row: see the modes) |
| ALGO | the step page (1-16, 17-32 ...); LOOP ENGINE (0.6.14): the step on the shown page, full left = LIVE; KEYBOARD: the target step |
| PRESETS | the track's pool slot; LOOP ENGINE and TRIG (0.6.14): the step page (the slot is the SLOT param) |
| K1-K4 | the page's four values |
| F1 / F2 | copy / cut and clear (below); in a pop-up: back / OK |
| GLO | SHIFT |
| PLAY | start / stop |
| REC | live recording; hold: the record settings |

## Global rules

These hold in every mode unless the mode says otherwise.

### White keys: steps

- **A quick tap** (under 0.25 s) puts a trig on the step or takes it off. A new trig keeps the key's velocity.
- **Hold a step + turn a knob** = lock that param on the step. Several held steps all get the lock.
- **Hold a step + turn PRESETS** = lock a pool slot on the step (the trig goes on too). The trig plays that slot; the
  next unlocked trig plays the track's own.
- Holding a trig step while stopped plays it (the step preview).
- **Locks stick, as on an Elektron (0.6.5).** A locked value holds after its note ends, until something changes it:
  - a **trig without that lock** puts the track value back (a ghost trig, with ERST / LRST / FRST all off, carries
    the values on);
  - a **lock-only step** (Elektron's trigless lock) sets only the params it locks;
  - **turning the knob** of that param while it plays is heard at once.
  Loop start / end locks still last only for their note. Stopping the transport puts every track value back.
- **SLIDE** (TRIG, second lane, K1; lock it ON on a step): each param the step locks slides to the value the next step
  that sets that param locks, arriving as that step plays. Nothing slides to a plain trig (it resets the param).
- **ALGO** picks the step page; the white keys then show that page. "PAGE 2/4" says where you are.
- LEDs: trig mid, a trig with locks flickers, a lock-only step dim, the playhead bright, held keys bright.

### Knobs

- **K1-K4** change the page's four values. A page with more than four has **lanes** (the header's box: P1, P2 ..).
- **One page list per mode** (0.6.2): every mode's pages, lanes and knob pages included, are one ordered list.
  **SEL walks it both ways** and stops at its first and last page (no wrap), so turning back always returns the way
  it came and SEL alone reaches everything in a mode. **A page key jumps to its group's first page; tapped again
  there it steps through the group's pages** (the same ones SEL passes), back to the first after the last. Nothing
  is reachable only by a key. The lists, as built (track selected):
  - TRIG: TRIG P1, TRIG P2 (B1), COND (B2), PLAYHEAD (B3)
  - LOOP ENGINE (0.6.41): TRIG P1-P4 (B1), LOOP P1, LOOP P2 (B2), MACHINE (B3), WARP (B4), RANGE (B5); the warp page's lanes follow the warp
  - FILTER: FILTER (B1; lanes by filter machine), FILTER ENV (B2), MIX (B3)
  - AMP: AMP P1, AMP P2 (B1), AMP ENV (B2), MOD ENV P1, MOD ENV P2 (B3)
  - FX (0.6.3): INSERT 1 P1.. (B1), INSERT 2 P1.. (B2), SEND 1 P1.. (B3), SEND 2 P1.. (B4), SENDS, MASTER P1.. (B5),
    FX MIX (B11); each effect has as many knob pages as its params need (three a page, K4 its MIX)
  - MODULATION: MATRIX (B1, AT), LFO 1 (B6), LFO 2 (B7), VELOCITY (B9), MACROS (B11), MACRO 1-4 (B2-B5)
  - MIX: TRACK MIX P1, P2, MASTER (no page keys: the black keys are the rates; MST selected shows MASTER)
  - FILE: BROWSE (B3), POOL (B6); 0.6.15: the EDITOR (P1, P2) and RAZOR (P1, P2) are sub-modes on the FILE button
  - PATTERN LAUNCH: the banks A-D (B1-B4); CROSSFADER, KEYBOARD, PAGES, SAMPLER: one page
  - With a bus selected, the modes that show the bus's page list only its lanes.
- **Lists** (0.6.1: APP SETTINGS, the record settings, every mode's settings, the recorder's setup): **SEL moves the
  one selected row, K1 changes its value**; K2-K4 do nothing there. The SETUP rows in the tables below are the list's
  rows in order (their KNOB columns are the 0.6.0 layout). A row whose label and value don't fit on one line shows the
  value on a second line (THEME always does).
- **Knobs accelerate:** turned slowly a click is the finest step; spun, the steps grow. Lists are one item a click.
  APP SETTINGS > KNOB SPEED sets how fast.
- **SHIFT + a value knob snaps** to useful values: the norns script's own snap lists where it has them (P/T -24 -12
  -7 0 7 12 24; STRT / END 0 8 16 32 64 96 120 128; CUT / RES / DRIV / MIX 0 32 64 96 127; VOL 0 32 64 100 127; LEN
  4 8 16 32 48 64 96 128; RATE 1/2 1 2; BPM 60 80 90 100 110 120 128 136 140 160 180 ...), else ChoralRoot's
  rules (0..127 to 0 16 32 .. 112 127, bipolar values to steps of 8, semitones to octaves). Lists don't snap.
- **PRESETS** walks the pool's **used** slots (empty ones are skipped), and you hear the change at once. The takeover
  shows the slot, the file's name, "3/12" (its place among the used slots) and how full the pool is. An empty pool
  says POOL EMPTY.
- **A value turned while its cell isn't on the screen** takes over the screen for a second: the value big, a bar,
  its name.

### F1 and F2 (OCT- and OCT+)

- **A tap = sticky on / off. A hold = on while held.** Only one is sticky at a time. The lit button blinks while
  sticky.
- **F1 on:** turn a knob = **clear every step lock of that param** on the selected track (with steps held: only on
  those steps). The white keys **copy / paste steps** (below). The knob labels say CLR.
- **F2 on:** turn a knob = **clear every modulation of that param** (the matrix slots aimed at it). The white keys
  **cut / paste steps**. The knob labels say UNMOD.
- **F1 + F2 together = F3** (below).
- They do something else in KEYBOARD (the octave), on the RAZOR page (split / delete), in FILE's browser (F2 frees a
  slot), FILE's EDITOR (F1 / F2: START / END, the point the white keys place), FILE's POOL (F1 MOVE, F2 FREE) and in
  every pop-up (F1 = back / cancel, F2 = OK / yes).

### SHIFT (GLO)

- **Held** = SHIFT while held. **A quick tap = sticky SHIFT**: it stays on until GLO is tapped again or the mode
  changes. The GLO LED blinks while sticky.
- 0.6.1: **held works exactly as sticky**: its screen shows from the press for as long as GLO is held, used or not,
  and every SHIFT knob and key works; letting go returns to the mode. A hold that was used (a knob, a key) never turns
  sticky on release; a quick unused tap does.
- While SHIFT is on, only its keys light and the panel labels show what they do.
- 0.6.6: **SHIFT's screen covers only the top** (BPM, swing, the track keys); the page's own knob boxes stay under
  it, so you see each param jump as SHIFT + its knob snaps it.

| SHIFT + | Does |
|---|---|
| SEL | BPM, in **0.1 steps** (faster turns move further) |
| ALGO | swing, one step a click |
| PRESETS | the target: T1..T6, S1, S2, MST (prev / next) |
| a value knob | **snap** that param on the page to its next useful value (the norns script's snap lists) |
| W1-W9 | select the target: T1-T6, S1, S2, MST (MIX, CROSSFADER, KEYBOARD: see there) |
| F1 / F2 | **undo / redo** (per gesture; the LEDs light when there is something to undo / redo) |
| B1 | **tap tempo** (the average of the last four taps) |
| B2 | **latch** fill on / off |
| B4 | **recall** the memorized project (lit once something is memorized) |
| B5 | **memorize** the project to RAM |
| B11 (held) | **FILL** while held |
| B3, B6-B10 | nothing (dark) |
| FILE | save the project |
| HOME | LOOP SETUP (0.6.15; APP SETTINGS: SHIFT + ARP) |
| SEQ | F3 |
| another mode button | that mode's settings |
| REC | the record settings |

### PLAY and REC

- **PLAY starts or stops at once, on the press.** Stop is a real stop (the next start is from step 1) and ends live
  recording. In FILE, PLAY does nothing.
- **REC** tapped (on release) = live recording on / off, in every mode but FILE (see Live recording). **Hold REC, or
  SHIFT + REC** = the record settings. In FILE, REC does nothing, and entering FILE ends recording.
- LEDs: PLAY green while playing. REC lit while recording, blinking while it waits for the first key or counts in,
  dark in FILE.

### Targets: tracks and buses

The **target** is a track T1-T6 or a bus S1, S2, MST. Pick it with SHIFT + W1-W9, SHIFT + PRESETS, or MIX's track
keys. With a bus selected, every mode but MIX, FILE, PATTERN LAUNCH and CROSSFADER shows that bus's page (its FX
machine's params); the bus pages are in the tables below.

### F3 (F1 + F2, or SHIFT + SEQ; 0.6.15: a held SEQ is the mode picker)

The timing menu. It stays open; F1, F2, HOME or SEQ close it. PLAY still plays and stops.

| Control | Does |
|---|---|
| SEL | the tempo |
| ALGO | the swing |
| PRESETS | **name the pattern** |
| K1 / K2 | the track's pattern LENGTH / RATE |
| K3 / K4 | the global length / LNCH MODE (the pattern launch quantize) |
| White key tapped | edit that step page |
| White key held | that page in / out of the playing loop |

### The header

0.6.1: there are **no message boxes**. Every message (STOP FIRST, COPY STEP 1, MEMORIZED, T1 MUTED, SAVING ...)
shows in the header's title line for its time (1.2 s); alerts (STOP FIRST, FULL, BUSY ...) fill the line with the
ALERT colour.

**While a key that does something when held is held, the header says what it does**, and goes back to the screen's
title when it is let go. The text shows 0.15 s into the hold, so a quick tap shows nothing; sticky SHIFT / F1 / F2
show at once. A message newer than the last key press wins over it; an older one gives way.

| Held | Mode | Header |
|---|---|---|
| B6-B10 (the sources) | FX, FILTER, AMP, MODULATION, LOOP ENGINE, TRIG | LFO 1 / LFO 2 / MOD ENV / VEL / AT, on the **SRC** (blue) bar |
| B2-B5 (the macros) | MODULATION | M1 / M2 / M3 / M4, on the SRC bar |
| a mode button (before its settings open at 0.4 s) | any | FX SETUP, FILTER SETUP, AMP SETUP, PROJECT SETUP (EDIT), LOOP SETUP (HOME), RECORDER SETUP (FILE), F3 (SEQ) |
| REC | any but FILE | REC SETUP |
| B11 | CROSSFADER | CAPTURE |
| B1 / B2 | CROSSFADER | SCENE A / SCENE B |
| W1-W12 | CROSSFADER | GLIDE TO n (0..127) |
| SHIFT + B11 | any | FILL |
| B1 (the quick setting) | FILTER / AMP / LOOP ENGINE / FX | FILTER TYPE / ENV MODE / MACHINE / FX TYPE |
| B3-B5 (the buses) | FX | S1 FX TYPE / S2 FX TYPE / MST FX TYPE |
| a key | KEYBOARD | NOTE C4 (the note), SLICE n (a slice key on slice tracks) |
| steps (white keys) | the step modes | STEP 5, or 3 STEPS, on the **LOCK** bar |
| a white key | LOOP ENGINE | LOOP KEY n (slice tracks: PAD n) |
| a black key | PAGES | LOOP PAGE n |
| a white key | FILE / EDITOR | START + KEY = END (F1), END + KEY = START (F2): what a second key places |
| a white key | F3 | LOOP PAGE n |
| W1-W6 | MIX | MUTE Tn (UNMUTE Tn when muted) |
| F1 (held or sticky) | the step modes / PAGES / PATTERN LAUNCH | F1 COPY/CLR LOCK; F1 COPY PAGE; F1 COPY PATTERN (PASTE ... once the buffer holds something); 0.6.12: shown at once, and sticky F1 fills the header's right box on the accent |
| F2 (held or sticky) | the step modes / PAGES / PATTERN LAUNCH | F2 CUT/CLR MOD; F2 CUT PAGE; F2 CUT PATTERN (PASTE ...); sticky F2 in the box |
| SHIFT (GLO, held or sticky) | any | SHIFT |

FX's insert key and LOOP ENGINE's B1-B5 open hold menus (their own text in the header).

### Pop-ups

F3, a settings list, APP SETTINGS, the record settings, the recorder's setup, naming and questions are pop-ups.
Inside one: **F1 = back / cancel, F2 = OK / yes, HOME closes**. F1 is lit and F2 blinks while one is open.

### Naming

F3 + PRESETS names the pattern; the project settings' NAME (hold EDIT) names the project (its file follows);
PROJECTS' B2 renames a project file. 0.6.47 (owner): **FIZZ's typewriter keybed** for all of them:

- **B1..B11 type A..K, W1..W16 type L..Z and SPACE**; the screen draws the keybed with a letter on every key (the key
  typed glows) and every key is lit. **SHIFT held**: the black keys 0..9 and -, the white keys . ' & ! # + ( ) _ ~ ,
  = @ $ % and SPACE (only characters a file name can hold). 14 characters (0.6.48: one font size on screen).
- **The suggested name starts selected** (a cream band): OK keeps it, **the first key typed replaces it**.
- **PRESETS (or K2 / K4, 0.6.48) right rolls a new name every detent** (namesizer pairs mostly, now and then a phonic or cosmic word:
  FIZZ's roller), the blue die tumbling under it as it rolls; **left walks back** through the last ten names (the dots under the name);
  right then goes forward through them before rolling new ones. A name you typed or edited is kept among them.
- **ALGO** moves the cursor (typing inserts there); **SEL / K1** turns the character under it (at the end: one more).
- **OCT-** deletes the character before the cursor (a selected name: all of it; an empty name: cancel);
  **SHIFT + OCT-** clears. **OCT+ (or FILE) = OK**, **HOME** cancels.
- A project or file name taken by another file gets the next free number.

## Copy / cut / paste (F1 / F2 on)

There is a one-item **buffer**. It works on steps (the white keys), on whole pages (PAGES' black keys) and on
patterns (PATTERN LAUNCH's white keys).

- **Copy (F1).** The first tap copies the step into the buffer. The next tap pastes it there, onto any step, even
  one that isn't empty.
- After that, a tap on a step that **isn't empty copies it** (a new buffer), and a tap on an **empty step pastes**.
- **Cut (F2)** is the same, but taking a step into the buffer empties it. So cut a step, tap another: it **moves**.
  Cut a step and tap it again: it goes back. Further taps on empty steps paste copies.
- **The buffer empties when F1 / F2 turns off.**
- The source step (or page) blinks while it is in the buffer. The header says what happened (COPY STEP 1, PASTE
  STEP 3 ...).
- Each paste and cut of steps or pages is one undo.

## The source template (FX, FILTER, AMP, MODULATION, LOOP ENGINE, TRIG)

| B1 | B2 | B3 | B4 | B5 | B6 | B7 | B8 | B9 | B10 | B11 |
|---|---|---|---|---|---|---|---|---|---|---|
| page 1 (hold: quick setting) | page 2 | page 3 | page 4 | page 5 | LFO 1 | LFO 2 | MOD ENV | VEL | AT | the mode's special key |

- **B1-B5 open the mode's pages** (the "Key" column of the tables below); tapped again on their page, they step
  through its lanes / knob pages (0.6.2). A key with no page is dark.
- **Hold B1 = the quick setting**, changed with K1 or SEL while held: FILTER the filter type, AMP the envelope mode.
- **Hold menus** (0.6.1) in FX and LOOP ENGINE: holding the key (400 ms, or at once with SEL / K1) shows a list of
  one setting's values over the screen, the header saying what the key does; SEL or K1 moves the cursor, letting go
  picks (nothing changes if the cursor never moved). A tap still opens the key's page (or steps its group).
  - LOOP ENGINE (0.6.41: one key on): B1 TRIG = ADVANCE, B2 LOOP = loop mode (the slice play mode on slice
    machines, else the chop loop mode), B3 MACHINE = machine (0.6.16: STOP FIRST while playing, like the warp), B4
    WARP = warp mode (STOP FIRST while playing), B5 RANGE = voice mode (MONO / POLY).
  - FX (0.6.3): B1 / B2 / B3 / B4 = INSERT 1's / INSERT 2's / SEND 1's / SEND 2's effect type; B5 on MASTER = the
    master's. On SENDS a held B1-B4 swaps instead (below).
- **FX** (0.6.3, docs/UX-V2.md §10 "FX layout"): two inserts per track, INSERT 1 then INSERT 2 in series (an empty
  slot costs nothing), and the S1 / S2 send buses. **EFFECTS** lists four rows, INSERT 1, INSERT 2, SEND 1, SEND 2
  (the effect, its level as a bar, its knob pages as little boxes, the one shown lit; the selected row highlighted).
  **B1-B4 select a row; tapped again they step its knob pages** (P1, P2 .., wrapping). K1-K3 are the effect's params
  in threes, **K4 always its MIX** (fx_wet where the effect has no dry / wet; on a send bus too). INSERT 2's params
  follow its machine (a new type starts from that type's defaults; one UNDO takes the pick back). **B5 steps SENDS ->
  MASTER P1 .. -> SENDS**: **SENDS** draws the track's routing (FILT -> INSERT 1 -> INSERT 2 -> AMP -> MIX, the send
  tap in accent where the send mode puts it, down to S1 and S2's effects and on to MST) and the four levels; its knobs
  are **K1 INSERT 1's MIX, K2 INSERT 2's MIX, K3 SEND 1, K4 SEND 2**, and **ALGO is the send mode PRE / POST**
  (p-lockable, undoable). PRE taps after the filter and the track's level and pan (the AMP block: applied at the
  filter's output), before the inserts; POST taps after both inserts. The routing boxes say only **FX 1, FX 2, SEND 1,
  SEND 2** (the level bars below show just the effect names), joined by one orange send path with square corners. **Moving an effect: hold its key on SENDS** (B1 FX 1, B2 FX 2, B3 SEND 1, B4 SEND 2): after 400 ms its box is
  **raised** (lifted, bright, a shadow under it) and the header says MOVE and the effect's name; nothing has moved
  yet. **Turn K1 while holding** to move it right or left: it slides over (160 ms) and both boxes keep their names
  while you hold (FX 1 moved right still says FX 1); moving an insert swaps INSERT 1 and INSERT 2 (machines,
  params, MIX, the step and slice locks and matrix routes on them, every pattern); moving a send swaps SEND 1 and
  SEND 2 (both buses' effects and settings, every track's send levels and their locks, the scenes: the sound stays
  the same). Let go to put it down: one UNDO puts the whole move back. Let go without turning K1: nothing changes.
  **MASTER** is the master bus's effect (its knob pages) and the **load meter** (the cart's counted load: 100 % =
  the hosts' 2x real-time budget). The header shows the place (FX/INSERT 2, P2). Targeting S1 / S2 / MST with the
  track keys shows that row / MASTER in FX.
- **FX settings** (hold FX): INSERT 1, INSERT 2, SEND 1, SEND 2, MASTER (each row that slot's effect type, the same
  choices as the type menus), then SEND MODE.
- **B6-B10 are the sources.** Hold one + turn any value knob = route that source to that param:
  - the first detent puts the route in a matrix slot (that source's slot on that param, else the first free one);
    the next detents set its DEPTH. The cell shows SOURCE > PARAM and the depth.
  - **F2 on while turning = remove that route.**
  - **Steps held as well = depth locks** on those steps.
  - Not a destination: **NOT MODDABLE** in the header (F2 + a knob on it says the same). All eight slots taken:
    **MATRIX FULL** (an alert).
  - Let go: a route this hold made and left at 0 is freed.
  - While held, the header shows the source's name on the SRC (blue) bar.
- **A source key never changes the mode**, tapped or held (0.6.1). A tap opens the source's page only when that page
  is in the mode shown: in MODULATION LFO 1, LFO 2 and VEL their pages and AT the MATRIX; in AMP, MOD ENV its MOD ENV
  page. Elsewhere a tap does nothing.
- B11 is the mode's special key (in AMP and TRIG it is unused).
- LEDs: the current page's key bright, the other page keys dim, the sources dim, the special key mid, a held key
  bright.

<!-- layout:begin (generated by tools/layout/gen_layout.py from design/layout/elasticat.json: edit the design, not this) -->

## Modes, pages and knobs (the layout "elasticat-0.6.0")

Each mode button opens its mode; tapping the lit button again goes to the mode's next page (HOME, EDIT and FILE: to the next mode on that button). In a mode, the black key a page names (Key) opens that page. A page has one or more lanes of four values on KNOB 1-4. Where a page depends on the track's machine, warp mode, filter machine, envelope mode or FX machine, each variant has its own rows. Settings: hold the mode button, or SHIFT + it.

| Button | Modes (in order) |
|---|---|
| FX | FX |
| SEL | FILTER |
| ENV | AMP |
| LFO | MODULATION |
| EDIT | MIX → PAGES (a fixed screen) |
| HOME | LOOP ENGINE → TRIG → KEYBOARD (a fixed screen) |
| FILE | FILE → PROJECTS (a fixed screen) → SAMPLER (a fixed screen) |
| ARP | CROSSFADER |
| SEQ | PATTERN LAUNCH |

### FX (FX · FX)

| Page | Key | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|---|
| EFFECTS · DELAY | B1 | 1 | TIME | FBK | TONE | MIX |
| EFFECTS · REVERB |  | 1 | SIZE | DAMP | MIX |  |
| EFFECTS · LOFI |  | 1 | BITS | RATE | MIX |  |
| EFFECTS · COMP |  | 1 | THRS | RATO | ATK | REL |
|  |  | 2 | MKUP | MIX |  |  |
| EFFECTS · DESTRY |  | 1 | CRSH | SRR | WARB | DRIV |
|  |  | 2 | FOLD | SAT | TONE | LEVL |
| EFFECTS · ECHO |  | 1 | TIME | OFST | FBK | MODE |
|  |  | 2 | TONE | WOBL | MIX |  |
| EFFECTS · B.HOLE |  | 1 | SIZE | GRAV | MOD | LOW |
|  |  | 2 | HI | PRE | MIX |  |
| EFFECTS · CHORUS |  | 1 | RATE | DPTH | WDTH | TONE |
|  |  | 2 | MIX |  |  |  |
| EFFECTS · FLANGR |  | 1 | RATE | DPTH | FBK | TONE |
|  |  | 2 | MIX |  |  |  |
| EFFECTS · PHASER |  | 1 | RATE | DPTH | CNTR | STGS |
|  |  | 2 | FBK | MIX |  |  |
| EFFECTS · DJ EQ |  | 1 | LOW | MID | HIGH | XLOW |
|  |  | 2 | XHI |  |  |  |
| EFFECTS · DUCK |  | 1 | AMNT | SENS | ATK | REL |
| EFFECTS · T.ECHO |  | 1 | TIME | FBK | WOW | FLUT |
|  |  | 2 | SAT | TONE | MIX |  |
| EFFECTS · CASSET |  | 1 | WOW | FLUT | NOIS | CRKL |
|  |  | 2 | DROP | TONE | MIX |  |
| EFFECTS · MOTION |  | 1 | WDTH | RATE | TREM | PAN |
|  |  | 2 | SHPE |  |  |  |
| EFFECTS · RINGS |  | 1 | MODE | FREQ | FINE | FBK |
|  |  | 2 | TONE | MIX |  |  |
| EFFECTS · LIMIT |  | 1 | GAIN | CEIL | REL |  |
| EFFECTS · otherwise |  | - | | | | |
| SENDS | B5 | 1 | INS 1 | - | SEND 1 | SEND 2 |
| MIX | B11 | 1 | FX MIX | DRIVE | SEND 1 | SEND 2 |
| S1 bus | B3 | | the bus's page (below) | | | |
| S2 bus | B4 | | the bus's page (below) | | | |
| SETUP | hold | 1 | INSERT 1 | - | SEND MODE |  |

### FILTER (SEL · FILT)

| Page | Key | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|---|
| FILTER · CLASSC | B1 | 1 | CUTOFF | RESO | TYPE | DRIVE |
| FILTER · MORPH |  | 1 | CUT | RES | MRPH | DPTH |
|  |  | 2 | DRIV |  |  |  |
| FILTER · LADDER |  | 1 | CUT | RES | DRIV | DPTH |
| FILTER · COMB |  | 1 | TUNE | FB | DAMP | DPTH |
|  |  | 2 | MIX |  |  |  |
| FILTER · FORMNT |  | 1 | CUT | RES | VOWL | DPTH |
|  |  | 2 | MIX |  |  |  |
| FILTER · SVF |  | 1 | CUT | RES | TYPE | DPTH |
|  |  | 2 | DRIV |  |  |  |
| FILTER ENV · F.ENV ADSR | B2 | 1 | ATK | DEC | SUS | REL |
|  |  | 2 | DPTH |  |  |  |
| FILTER ENV · otherwise |  | 1 | ATTACK | HOLD | RELEASE | DEPTH |
| MIX | B3 | 1 | STEREO BAL | WET |  |  |
| SETUP | hold | 1 | FILT MODE | ENV MODE | MIX MODE |  |

### AMP (ENV · AMP)

| Page | Key | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|---|
| AMP | B1 | 1 | LEVEL | PAN | VELOCITY | VEL DEPTH |
|  |  | 2 | SEND 1 | SEND 2 |  |  |
| AMP ENV · ENV ADSR | B2 | 1 | ATCK | DECAY | SUST | REL |
| AMP ENV · otherwise |  | 1 | ATTACK | HOLD | - | RELEASE |
| MOD ENV | B3 | 1 | ATTACK | DECAY | SUST | RELEASE |
|  |  | 2 | - | - | - | DEPTH |
| SETUP | hold | 1 | ENV MODE | ENV RANGE | PORTAMENTO | SLICE HOLD |
|  |  | 2 | HOLD TIME |  |  |  |

### MODULATION (LFO · MOD)

| Page | Key | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|---|
| MATRIX | B1 | 1 | SRC | DST | DEPTH |  |
| LFO 1 |  | 1 | WAVE | SPEED | DEPTH | MODE |
| LFO 2 |  | 1 | WAVE | SPEED | DEPTH | MODE |
| VELOCITY |  | 1 | - | - | - | DEPTH |
| MACROS | B11 | 1 | M1 | M2 | M3 | M4 |
| MACRO 1 | B2 | 1 | VAL |  |  |  |
| MACRO 2 | B3 | 1 | VAL |  |  |  |
| MACRO 3 | B4 | 1 | VAL |  |  |  |
| MACRO 4 | B5 | 1 | VAL |  |  |  |

### MIX (EDIT, 1st · MST)

| Page | Key | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|---|
| TRACK MIX |  | 1 | LEVEL | PAN | SEND 1 | SEND 2 |
|  |  | 2 | MUTE |  |  |  |
| MASTER |  | 1 | BPM | SWNG | GLO LENGTH |  |
| SETUP | hold | 1 | NAME | GLO BPM | BPM | RETURNMODE |

### LOOP ENGINE (HOME, 1st · SRC)

| Page | Key | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|---|
| TRIG · SLICE/RAZOR | B1 | 1 | NOTE | LEN | VEL | MICRO |
|  |  | 2 | SLIDE | ENV RESET | LFO RESET | FILT RESET |
|  |  | 3 | CHANCE | COND | RATCHET | - |
|  |  | 4 | SLIC | PLAY | RTCH | SREV |
| TRIG · otherwise |  | 1 | NOTE | LEN | VEL | MICRO |
|  |  | 2 | SLIDE | ENV RESET | LFO RESET | FILT RESET |
|  |  | 3 | CHANCE | COND | RATCHET | - |
|  |  | 4 | SNAP START | RET MODE |  |  |
| LOOP · NEIGHBOR/INPUT | B2 | - | | | | |
| LOOP · SLICE/RAZOR |  | 1 | P/T | SLOT | SLIC | PLAY |
|  |  | 2 | CNT | CHOK | SMUT | SREV |
| LOOP · otherwise |  | 1 | PITCH | SLOT | START | END |
|  |  | 2 | XFADE | REVERSE |  |  |
| MACHINE · NEIGHBOR/INPUT | B3 | - | | | | |
| MACHINE · SLICE/RAZOR |  | 1 | CNT | PLAY | SREV | RTCH |
|  |  | 2 | SYNC | RATE |  |  |
| MACHINE · otherwise |  | 1 | LREVERSE | LXFADE | RET MODE |  |
| WARP · NEIGHBOR/INPUT | B4 | - | | | | |
| WARP · TAPE |  | 1 | WARP RATE | WARP FADE |  |  |
| WARP · VARI |  | 1 | RATE | MFAD |  |  |
| WARP · DOMINO |  | 1 | SLCS | SLEN | LOOP | RATE |
|  |  | 2 | ATK | REL | GATE |  |
| WARP · GRAIN |  | 1 | GDEN | GSIZ | SPRD | RATE |
|  |  | 2 | GSPD | RAND | DIR |  |
| WARP · OLA |  | 1 | CHAOS | OWIN | OWAN | RATE |
| WARP · PITCH |  | 1 | SMEAR | PCWN | PCDS | RATE |
| WARP · HARM |  | 1 | MIX | IN1 | IN2 | IN3 |
|  |  | 2 | RATE |  |  |  |
| WARP · WTBL |  | 1 | MORF | WSIZ | CYCW | LRAT |
|  |  | 2 | LDEP | LSHP | XFAD | RATE |
| WARP · FREEZE |  | 1 | FRZE | BLUR | RATE |  |
| WARP · FORMNT |  | 1 | FORM | FRZE | RATE |  |
| WARP · GSTR |  | 1 | STRCH | PWIN | SMTH | RATE |
| WARP · GSTR2 |  | 1 | STRCH | PWIN | SMTH | RATE |
| WARP · CHOP |  | 1 | SLEN | GATE | RATE | - |
|  |  | 2 | ATK | REL |  |  |
| RANGE · NEIGHBOR/INPUT | B5 | - | | | | |
| RANGE · otherwise |  | 1 | - | - | R-START | R-END |
| SETUP | hold | 1 | MACHINE | WARP MODE | LOOP DIV | VOICE MODE |

### TRIG (HOME, 2nd · TRIG)

| Page | Key | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|---|
| TRIG | B1 | 1 | NOTE | LEN | VEL | MICRO |
|  |  | 2 | SLIDE | ENV RESET | LFO RESET | FILT RESET |
| COND | B2 | 1 | CHANCE | COND | RATCHET |  |
| PLAYHEAD (as SLICE) · SLICE/RAZOR | B3 | 1 | SLIC | PLAY | RTCH | SREV |
| PLAYHEAD · otherwise |  | 1 | SNAP START | RET MODE |  |  |
| SETUP | hold | 1 | BPM | SWING |  |  |

### FILE (FILE, 1st · FILE)

| Page | Key | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|---|
| BROWSE | B7 | - | | | | |
| EDITOR |  | 1 | START | END | - | ZOOM |
|  |  | 2 | BPM | STEPS | GAIN | ZOOM |
| SLICE |  | 1 | SLCE START | SLCE END | PLAY | ZOOM |
|  |  | 2 | SNAP MODE | CHOKE | PITCH | ZOOM |
|  |  | 3 | COUNT | TRANS SENS | - | ZOOM |
| POOL | B6 | - | | | | |

### CROSSFADER (ARP · XFD)

| Page | Key | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|---|
| SCENE |  | 1 | CROSSFADE |  |  |  |

### PATTERN LAUNCH (SEQ · PAT)

| Page | Key | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|---|
| PATTERN |  | 1 | QUANT | LENGTH |  |  |
| SETUP | hold | 1 | LNCH MODE | GLO LENGTH |  |  |

### Buses (S1, S2, MST: every mode but MIX, FILE, PATTERN LAUNCH and CROSSFADER shows the bus's page)

| Page | Key | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|---|
| S1 SETUP | hold | 1 | SEND 1 FX |  |  |  |
| S2 SETUP | hold | 1 | SEND 2 FX |  |  |  |
| MST SETUP | hold | 1 | MASTER FX |  |  |  |
| S1: SEND 1 | B3 | | the lanes of FX/EFFECTS, for the bus's own FX machine | | | |
| S2: SEND 2 | B4 | | the lanes of FX/EFFECTS, for the bus's own FX machine | | | |
| MST: MASTER |  | | the lanes of FX/EFFECTS, for the bus's own FX machine | | | |
| MST: LOOP ENGINE = MIX |  | 1 | VOL | PAN | MUTE | BPM |

### F3

| Page | Key | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|---|
| F3 |  | 1 | LENGTH | RATE | GLO LENGTH | LNCH MODE |

<!-- layout:end -->

<!-- params:begin (generated by tools/layout/gen_layout.py from design/layout/descriptions.json: edit that, not this) -->

## Parameter reference

What every value does, grouped by the mode whose pages show it first (the designer shows the same text as tooltips). Label: the screen's own short name (a page may give it another).

### TRIG (TRIG)

TRIG (HOME, second): the step's own settings (note, length, velocity, resets), trig conditions and ratchets, and the trig-jump / slice page. B1-B3 open its pages. Settings: tempo, swing.

| Item | Label | Name | What it does |
|---|---|---|---|
| `pitch` | P/T | Pitch | Transposes the track, -24 to +24 semitones; on TRIG it is the step's note. P-lockable. In TAPE it is speed too; VARI ignores it. |
| `slice_index` | SLIC | Slice | Which slice the track or step plays, 1 to 32. P-lockable per step. |
| `slice_play_mode` | PLAY | Slice play mode | 1SHOT (to the end), HOLD (while the note lasts), LOOP, CONT (on past the slice), PONG (back and forth) or CLOOP (continue, looping). P-lockable. |
| `slice_reverse` | SREV | Slice reverse | Plays slices backwards. P-lockable. |
| `slice_ratchet` | RTCH | Slice ratchet | Repeats the slice 1 to 4 times within the step (multiplies the trig ratchet). P-lockable. |
| `trig_jump` | JUMP | Trig jump | ON: a trig snaps the playhead to its region's start. OFF: the playhead keeps its position, jumping only if it falls outside the region. P-lockable. |
| `trig_release` | RLSE | Trig release | When a step's region lock ends: RETURN (back in sync, where the loop would be), BOOMR (plays back out in reverse) or RESET (from the start). P-lockable. |
| `trig_chance` | CHNC | Trig chance | The probability a step fires, 0 to 100%. P-lockable per step. |
| `trig_condition` | COND | Trig condition | When a step fires: -- (always), A:B (the A-th of every B passes), FILL / !FILL, PRE / !PRE (previous conditional passed), NEI / !NEI, 1ST (first pass only). |
| `trig_ratchet` | RTCH | Trig ratchet | Repeats the step 1 to 8 times, evenly spread over the step. P-lockable. |
| `env_reset` | ERST | Envelope reset | ON: the step retriggers the mod envelope. With ERST, LRST and FRST all off the step is a ghost trig (no new note). P-lockable. |
| `lfo_reset` | LRST | LFO reset | ON: the step restarts the LFOs that are in TRIG, ONE or HOLD mode. P-lockable. |
| `filter_reset` | FRST | Filter reset | No effect of its own (a placeholder, as in the original), but with ERST and LRST also off the step becomes a ghost trig. P-lockable. |
| `default_length` | LEN | Note length | How long a step's note lasts, 1/4 to 16 steps: the envelope gate, and how long its region locks hold (its other p-locks stick until a trig or lock changes them, as Elektron's). P-lockable. |
| `default_velocity` | VEL | Velocity | Trig velocity, 0 to 100%. Sets a slice's level and feeds the VELOCITY mod source (slice trigs only). P-lockable. |
| `micro_timing` | MICRO | Micro timing | A trig early or late by n/24 of a step (-12..+12). Live REC writes it from each hit's offset (unless QUANTIZE is HARD). P-lockable. |
| `trig_slide` | SLDE | Slide | ON (as a step lock): each param this step locks slides to the value the next step that sets it locks, arriving as that step plays (Elektron's parameter slide). Nothing slides to a plain trig, which resets to the track value. |
| `global.bpm` | BPM | Tempo | The project tempo, 20 to 300 BPM (SHIFT snaps to common tempos). Saved per pattern unless GBPM is on. |
| `global.swing` | SWNG | Swing | Global swing, 50% (straight) to 75% (heavy): odd steps get longer, even ones shorter. |

### LOOP ENGINE (SRC)

LOOP ENGINE (HOME, first; the power-on mode): the track's sound source - pitch, loop region, sample slot, machine and warp-mode controls over the waveform. B1-B4 open its pages. Settings: machine, warp mode, loop division, voice mode.

| Item | Label | Name | What it does |
|---|---|---|---|
| `machine` | MACH | Machine | What the track plays: LOOP, TRIG, SLICE, RAZOR, S.POLY, R.POLY, NEIGH (the left track) or INPUT (the live input). A setting, not p-lockable. |
| `sample_slot` | SLOT | Sample slot | Which of the 128 pool slots the track plays; OFF (the default) plays nothing. Only you set it (here, PRESETS or POOL's ASSIGN): loading a file never does. Tracks sharing a slot share its sample and its file settings. |
| `loop_start` | STRT | Loop start | Where the loop region starts, 0 to 128 across the Range. P-lockable (a step can play its own region). |
| `loop_end` | END | Loop end | Where the loop region ends, 0 to 128 across the Range. P-lockable (a step can play its own region). |
| `range_start` | R-ST | Range start | The start of the Range: a crop (never a repitch) of the trimmed, stretched sample; the loop points, loop keys and slices work in 0 to 128 of it. P-lockable. |
| `range_end` | R-EN | Range end | The end of the Range: a crop (never a repitch) of the trimmed, stretched sample; the loop points, loop keys and slices work in 0 to 128 of it. P-lockable. |
| `loop_reverse` | REV | Loop reverse | Plays the loop region backwards. In DOMINO it reverses the slice order. P-lockable. |
| `xfade` | XFAD | Loop crossfade | Crossfade time at the loop seam and on region jumps, 0 to 250 ms; in WTBL, the fade when the region moves. Longer = smoother, softer edges. |
| `mode` | WARP | Warp mode | How a loop machine reads its region: TAPE, VARI, DOMINO, GRAIN, OLA, PITCH, HARM, WTBL, FREEZE, FORMNT, GSTR, GSTR2 or CHOP. Changes only while stopped. |
| `mode_macro` | MACR | Warp macro | The warp mode's own amount, 0 to 100%: CHAOS in OLA, SMEAR in PITCH (wider window), harmony MIX in HARM, MORF (wavetable scan) in WTBL. |
| `chop_steps` | CHOP | Domino slices | DOMINO: how many equal slices the region is cut into, 1 to 64. |
| `chop_slice_len` | SLEN | Slice length | DOMINO and CHOP: steps per slice / grain, 0.05 to 32 (SHIFT snaps to whole steps). 1 = a new slice every step. |
| `chop_loop_mode` | LOOP | Slice loop mode | DOMINO: what each slice does inside its slot - CHOP (stop), LOOP, PONG or RUN (run on past the slice), each also reversed (R). Not on CHOP, which has no slice loop (nor has the original's CHOPPED). |
| `chop_attack` | ATK | Slice attack | DOMINO and CHOP: fade-in of each slice / grain, 0.1 to 200 ms. |
| `chop_hold` | GATE | Slice gate | DOMINO and CHOP: how much of each slot the slice sounds, 0 to 100%. Lower = rhythmic gating, full = smooth. |
| `chop_release` | REL | Slice release | DOMINO and CHOP: fade-out of each slice / grain, 0.1 to 200 ms. |
| `grain_size` | GSIZ | Grain size | GRAIN: the length of each grain, 2 to 500 ms. In GSTR / GSTR2 (PWIN): the pitch shifter's window; bigger = smoother pitch, more smear. |
| `grain_density` | GDEN | Grain density | GRAIN: how many grains overlap, 1 to 64. In GSTR / GSTR2 (SMTH): smoothing; higher = less metallic, a little softer. |
| `grain_jitter` | SPRD | Grain spread | GRAIN: random scatter of where grains start around the emitter, none to wide. 0 = all from the same point. |
| `grain_speed` | GSPD | Grain speed | GRAIN: how fast the emitter moves through the region, 0 (frozen) to 4x. In GSTR / GSTR2 (STRCH): the stretch, 1/16x to 4x, 1 = real time. |
| `grain_speed_rand` | RAND | Grain speed random | GRAIN: random variation of each grain's playback speed, 0 to 100%. |
| `grain_direction` | DIR | Grain direction | GRAIN: the share of grains that play forwards, 0 to 100%. 100% = all forward, 0% = all backward. |
| `warp_rate` | RATE | Warp rate | Tempo-synced speed multiplier for the warp mode's read, 1/16x to 8x; 1 = in time. P-lockable. |
| `slice_count` | CNT | Slice count | SLICE / S.POLY: how many equal slices the region is cut into, 1 to 32. |
| `slice_sync` | SYNC | Slice clock sync | ON: slice speed follows the clock (tempo-fitting warp modes stretch slices to the tempo). OFF: slice RATE sets the speed. |
| `slice_rate` | RATE | Slice rate | Slice playback speed when SYNC is off, 1/8x to 8x. P-lockable. |
| `mode_switch_fade` | MFAD | Mode switch fade | Crossfade time when the warp mode's sound changes over, 1 to 250 ms. |
| `loop_division` | LDIV | Loop key division | How many loop keys the region is divided into, 2 to 32 (even); above 16, SHIFT + key reaches 17-32. |
| `trig_polyphony` | POLY | Trig polyphony | MONO: a new trig ends the previous note's p-locks and region lock. POLY: each note keeps its locks until it ends. |
| `harm_interval` | IN1 | Harmony 1 interval | HARM: the first harmony voice, -24 to +24 semitones; 0 = off. P-lockable. |
| `harm_interval2` | IN2 | Harmony 2 interval | HARM: the second harmony voice, -24 to +24 semitones; 0 = off. P-lockable. |
| `harm_interval3` | IN3 | Harmony 3 interval | HARM: the third harmony voice, -24 to +24 semitones; 0 = off. P-lockable. |
| `wsola_window` | OWIN | OLA window | OLA: the length of each grain, up to 500 ms (settings under 30 ms act as 30 ms). P-lockable. |
| `wsola_search` | OWAN | OLA wander | OLA: how far grains wander from the playhead, 0 to 100 ms (more with CHAOS). P-lockable. |
| `pv_window` | PCWN | Pitch window | PITCH: the pitch shifter's window, 5 ms to 2 s (widened by SMEAR). Bigger = smoother, more smeared. P-lockable. |
| `pv_dispersion` | PCDS | Pitch dispersion | PITCH: random pitch scatter in the shifter, 0 to 100%. P-lockable. |
| `wt_window` | WSIZ | Wavetable size | WTBL: how many single-cycle waves are spread across the loop region, 2 to 64. P-lockable. |
| `wt_cycle` | CYCW | Cycle width | WTBL: the length of each single cycle, 16 to 8192 samples (moves in semitone ratios). P-lockable. |
| `wt_lfo_rate` | LRAT | Scan LFO rate | WTBL: the speed of the built-in scan LFO, 0 to 8000 Hz (audio rates give FM / AM textures). P-lockable. |
| `wt_lfo_depth` | LDEP | Scan LFO depth | WTBL: how far the scan LFO moves MORF, -64 to +64; 0 = off. P-lockable. |
| `wt_lfo_shape` | LSHP | Scan LFO shape | WTBL: the scan LFO's wave - SINE, TRI, SAW, S&H or RAND. P-lockable. |
| `freeze_amount` | FRZE | Freeze | FREEZE and FORMNT: captures and holds the spectrum at the playhead, 0 (live) to 100%. P-lockable. |
| `spectral_blur` | BLUR | Spectral blur | FREEZE: smears the held spectrum, 0 to 100%. P-lockable. |
| `formant_shift` | FORM | Formant shift | FORMNT: shifts the spectral envelope, -24 to +24 semitones; 0 = unchanged. P-lockable. |
| `e5.choke` | CHOK | Slice choke group | The choke group of the held pads (else the SLIC slice) in the pattern's slice program, NONE or 1 to 8; overrides the razor table's. |
| `e5.mute` | SMUT | Slice mute | Mutes the held pads (else the SLIC slice) in the pattern's slice program; muted slices stay on their steps but play nothing. |

### FILTER (FILT)

FILTER (SEL): the track's filter, its envelope and its stereo / mid-side spread. B1-B3 open its pages. Settings: filter machine, filter envelope mode, mix mode.

| Item | Label | Name | What it does |
|---|---|---|---|
| `filter_machine` | MACH | Filter machine | The filter type: CLASSC, MORPH, LADDER, COMB, FORMNT or SVF. Changes only while stopped. |
| `filter_type` | TYPE | Filter type | CLASSC and SVF: low-pass, high-pass, band-pass or notch. P-lockable. |
| `filter_cutoff` | CUT | Filter cutoff | Cutoff, 20 Hz to 20 kHz. On COMB (TUNE) the comb's pitch; on FORMNT it transposes the vowel formants. P-lockable. |
| `filter_res` | RES | Filter resonance | Resonance, off to full. On COMB (FB) the comb's feedback. P-lockable. |
| `filter_drive` | DRIV | Filter drive | Saturation before the filter, off to full. On COMB (DAMP) a low-pass in the feedback, darker each echo. No drive on FORMNT. P-lockable. |
| `filter_morph` | MRPH | Filter morph | MORPH: sweeps low-pass (0) through notch (centre) to high-pass (128). On FORMNT (VOWL): the vowel, A-E-I-O-U. P-lockable. |
| `filter_balance` | BAL | Filter balance | Spreads the two channels' cutoffs (L / R, or mid / side) up to +-24 semitones apart; 0 = both alike. P-lockable. |
| `filter_mix` | MIX | Filter mix | Dry / wet blend, dry to fully filtered. Only COMB and FORMNT use it. P-lockable. |
| `filter_mix_mode` | MODE | Filter mix mode | How every filter machine splits the signal: STEREO (left / right) or M/S (mid / side), spread by BAL. Changes only while stopped. |
| `filter_env_mode` | FENV | Filter envelope mode | ADSR (follows the note length) or AHR (attack, hold, release; ignores the note length). |
| `filter_env_attack` | ATK | Filter env attack | The filter envelope's attack time, 0 up to the envelope range (AMP setup). P-lockable. |
| `filter_env_decay` | DEC | Filter env decay | ADSR: the time to fall to the sustain level, 0 up to the envelope range. P-lockable. |
| `filter_env_sustain` | SUS | Filter env sustain | ADSR: the level held while the note lasts, 0 to full. P-lockable. |
| `filter_env_release` | REL | Filter env release | The release time after the note ends (ADSR) or after the hold (AHR); the top of the knob is infinite. P-lockable. |
| `filter_env_hold` | HOLD | Filter env hold | AHR: how long the envelope holds at full; the top of the knob is infinite. P-lockable. |
| `filter_env_depth` | DPTH | Filter env depth | How far the filter envelope moves the cutoff, up to +-72 semitones; centre 0 = none, below opens downward. P-lockable. |

### AMP (AMP)

AMP (ENV): the track's level, pan, velocity and sends, the amp envelope and the mod envelope. B1-B3 open its pages. Settings: envelope mode and range, portamento, slice hold.

| Item | Label | Name | What it does |
|---|---|---|---|
| `slice_hold_to_step` | SHLD | Slice hold to step | ON: a slice note lasts the step's note length (LEN). OFF: it lasts the HOLD time instead. |
| `slice_hold` | HOLD | Slice hold time | How long a slice note lasts when SHLD is off, 0 to 4 seconds. |
| `env_mode` | ENV | Amp envelope mode | ADSR (sustains while the note lasts) or AHR (attack, hold, release; ignores the note length). A setting, not p-lockable. |
| `env_attack` | ATCK | Amp attack | The amp envelope's fade-in, 0 up to the envelope range. P-lockable. |
| `env_decay` | DECAY | Amp decay | ADSR: the time to fall to the sustain level, 0 up to the envelope range. P-lockable. |
| `env_sustain` | SUST | Amp sustain | ADSR: the level held while the note lasts, 0 to full. P-lockable. |
| `env_release` | REL | Amp release | The fade-out after the note ends (ADSR) or after the hold (AHR); the top is INF (no fade; on slice voices a 20 ms release when the slice ends). P-lockable. |
| `env_hold` | HOLD | Amp hold | AHR: how long the sound stays at full before releasing; the top is INF (holds until the next trig). P-lockable. |
| `env_range` | RANGE | Envelope range | The longest time the envelope knobs reach, 1 s to 1024 s (default 8 s). Applies to the amp, filter and mod envelopes. |
| `send1_level` | SND1 | Send 1 level | How much of the track goes to the S1 bus, off to full. P-lockable: throw single steps into the effect. |
| `send2_level` | SND2 | Send 2 level | How much of the track goes to the S2 bus, off to full. P-lockable. |
| `amp` | VOL | Volume | The track's level, 0 to 127 (default 100), times the slot's gain. P-lockable. |
| `pan` | PAN | Pan | Stereo position, L64 to R64; C = centre. P-lockable. |
| `portamento` | PORT | Portamento (legato) | ON: a trig that arrives while the amp envelope is still open extends the note instead of restarting the attack. |
| `menv_attack` | ATCK | Mod env attack | The mod envelope's attack, 0 up to the envelope range. P-lockable. |
| `menv_decay` | DECAY | Mod env decay | The time to fall to the sustain level, 0 up to the envelope range. P-lockable. |
| `menv_sustain` | SUST | Mod env sustain | The level held while the note lasts, 0 to full. P-lockable. |
| `menv_release` | REL | Mod env release | The time to fall back after the note ends, 0 up to the envelope range. P-lockable. |
| `menv_depth` | DEPTH | Mod env depth | The mod envelope's level into the mod matrix (source ENV), -64 to +64; 0 = silent. Each slot's DEPTH scales it. P-lockable. |
| `mvel_depth` | DEPTH | Velocity depth | Velocity's level into the mod matrix (source VEL), -64 to +64; 0 = silent. Each slot's DEPTH scales it. P-lockable. |

### FX (FX)

FX (FX): the track's insert effect (B1), its sends (B2), the S1 / S2 / master buses' effects (B3 B4 B5) and the FX mix (B11). Settings: the FX machine and the send tap.

| Item | Label | Name | What it does |
|---|---|---|---|
| `fx_insert1_machine` | MACH | FX machine | The effect in this slot: the track's insert, or the S1 / S2 return or master insert. 18 machines; changes any time with a short fade. |
| `fx_drive` | DRIV | Destroy drive | DESTRY: the overdrive stage, off to full (0 bypasses it). P-lockable. |
| `fx_mix` | MIX | FX mix | Dry / wet blend of the effect, dry to fully wet. P-lockable. |
| `send_tap` | TAP | Send tap | Where the sends take the track's signal: PRE (after the filter, before the insert) or POST (after the insert). |
| `delay_time` | TIME | Delay time | DELAY: the echo time, 1/32 to 2 bars incl. dotted and triplet, synced to the tempo. P-lockable. |
| `delay_feedback` | FBK | Feedback | DELAY, ECHO, T.ECHO: how much of each repeat feeds back, off to full. P-lockable. |
| `delay_tone` | TONE | Delay tone | DELAY: low-pass on the repeats, 220 Hz to 18 kHz; full = brightest. P-lockable. |
| `reverb_size` | SIZE | Reverb size | REVERB and B.HOLE: the room size, small to huge. P-lockable. |
| `reverb_damp` | DAMP | Reverb damping | REVERB: high-frequency damping, bright to dark. On B.HOLE (HI): the tail's high cut. P-lockable. |
| `lofi_bits` | BITS | Bit depth | LOFI: the output bit depth, 1 to 24 bits; lower = more crushed. P-lockable. |
| `lofi_rate` | RATE | Sample rate | LOFI: the output sample rate, 1 kHz to 48 kHz; lower = more aliasing. P-lockable. |
| `comp_thresh` | THRS | Threshold | COMP: the level above which it compresses, about -34 to 0 dB. P-lockable. |
| `comp_ratio` | RATO | Ratio | COMP: how hard it compresses, 1:1 to 20:1. P-lockable. |
| `comp_attack` | ATK | Comp attack | COMP: how fast it clamps down, 0.5 to 100 ms. P-lockable. |
| `comp_release` | REL | Comp release | COMP: how fast it lets go, 20 to 800 ms. P-lockable. |
| `comp_makeup` | MKUP | Makeup gain | COMP: gain after compression, 0 to +12 dB. P-lockable. |
| `destroy_warble` | WARB | Warble | DESTRY: pitch wobble, off to fast and deep. P-lockable. |
| `destroy_fold` | FOLD | Wavefold | DESTRY: wavefolding, off to heavy. P-lockable. |
| `destroy_sat` | SAT | Saturation | DESTRY: saturation, off to heavy. P-lockable. |
| `destroy_crush` | CRSH | Bitcrush | DESTRY: bit reduction, off to 2 bits. P-lockable. |
| `destroy_srr` | SRR | Sample-rate reduction | DESTRY: sample-rate reduction, off to 500 Hz. P-lockable. |
| `destroy_tone` | TONE | Destroy tone | DESTRY: low-pass after the chain, 400 Hz to 18 kHz; full = brightest. P-lockable. |
| `destroy_level` | LEVL | Destroy level | DESTRY: output level, 0.2x to 2x. P-lockable. |
| `echo_time` | TIME | Echo time | ECHO and T.ECHO: the echo time, 1/32 to 2 bars incl. dotted and triplet, synced to the tempo. P-lockable. |
| `echo_offset` | OFST | Echo offset | ECHO: the right tap's time against the left, 0.5x to 2x; centre = equal. P-lockable. |
| `echo_mode` | MODE | Echo mode | ECHO: STEREO or PING (ping-pong between the sides). P-lockable. |
| `echo_tone` | TONE | Echo tone | ECHO and T.ECHO: low-pass on the repeats, dark to bright. P-lockable. |
| `echo_wobble` | WOBL | Echo wobble | ECHO: wobble of the echo time, off to strong. P-lockable. |
| `bh_gravity` | GRAV | Gravity | B.HOLE: the decay, short to infinite; at maximum the tail freezes. P-lockable. |
| `bh_mod` | MOD | Blackhole mod | B.HOLE: modulation of the reverb, off to deep. P-lockable. |
| `bh_low` | LOW | Low cut | B.HOLE: high-pass on the tail, 20 Hz to 600 Hz. P-lockable. |
| `bh_predelay` | PRE | Pre-delay | B.HOLE: delay before the reverb, 0 to 200 ms. P-lockable. |
| `modfx_rate` | RATE | Mod rate | CHORUS, FLANGR, PHASER: the sweep speed, slow to fast (chorus about 0.05-8 Hz, flanger / phaser 0.02-4 Hz). P-lockable. |
| `modfx_depth` | DPTH | Mod depth | CHORUS, FLANGR, PHASER: how deep the sweep goes, off to full. P-lockable. |
| `modfx_tone` | TONE | Mod tone | CHORUS, FLANGR, RINGS: low-pass on the wet signal, 400 Hz to 18 kHz. P-lockable. |
| `chorus_width` | WDTH | Chorus width | CHORUS: the phase offset between the left and right voices, mono to wide. P-lockable. |
| `flanger_feedback` | FBK | Flanger feedback | FLANGR: bipolar feedback; centre = none, up = metallic, down = hollow (negative). P-lockable. |
| `phaser_center` | CNTR | Phaser centre | PHASER: the sweep's centre frequency, 100 Hz to 4 kHz. P-lockable. |
| `phaser_stages` | STGS | Phaser stages | PHASER: how many all-pass stages, 1 to 8 (more = more notches). P-lockable. |
| `phaser_feedback` | FBK | Phaser feedback | PHASER: resonance, off to strong. P-lockable. |
| `eq_low` | LOW | EQ low | DJ EQ: the low band's gain; 0 = kill, centre = unity, top = boost. P-lockable. |
| `eq_mid` | MID | EQ mid | DJ EQ: the mid band's gain; 0 = kill, centre = unity, top = boost. P-lockable. |
| `eq_high` | HIGH | EQ high | DJ EQ: the high band's gain; 0 = kill, centre = unity, top = boost. P-lockable. |
| `eq_xlow` | XLOW | Low crossover | DJ EQ: where low meets mid, 80 to 500 Hz. P-lockable. |
| `eq_xhi` | XHI | High crossover | DJ EQ: where mid meets high, 1 to 8 kHz. P-lockable. |
| `duck_amount` | AMNT | Duck amount | DUCK: the most it turns the slot down, 0 to -40 dB. P-lockable. |
| `duck_sens` | SENS | Duck sensitivity | DUCK: how strongly the program level drives the ducking, low to high. P-lockable. |
| `duck_attack` | ATK | Duck attack | DUCK: how fast it ducks, 1 to 50 ms. P-lockable. |
| `duck_release` | REL | Duck release | DUCK: how fast it comes back, 50 ms to 1.2 s. P-lockable. |
| `tape_wow` | WOW | Wow | T.ECHO and CASSET: slow pitch wobble, off to strong. P-lockable. |
| `tape_flutter` | FLUT | Flutter | T.ECHO and CASSET: fast pitch wobble, off to strong. P-lockable. |
| `tape_sat` | SAT | Tape saturation | T.ECHO: saturation in the feedback loop, off to heavy. P-lockable. |
| `cass_noise` | NOIS | Hiss | CASSET: tape hiss, off to loud. P-lockable. |
| `cass_crackle` | CRKL | Crackle | CASSET: crackle density, off to dense. P-lockable. |
| `cass_drop` | DROP | Dropouts | CASSET: level dropouts, off to frequent and deep. P-lockable. |
| `cass_tone` | TONE | Cassette tone | CASSET: low-pass, 1.2 kHz to 16 kHz; full = brightest. P-lockable. |
| `motion_width` | WDTH | Stereo width | MOTION: 0 = mono, centre = unchanged, top = extra wide. P-lockable. |
| `motion_rate` | RATE | Motion rate | MOTION: the tremolo / autopan speed, 1/32 to 4 bars, synced to the tempo. P-lockable. |
| `motion_trem` | TREM | Tremolo | MOTION: tremolo depth (level), off to full. P-lockable. |
| `motion_pan` | PAN | Autopan | MOTION: autopan depth (left / right), off to full. P-lockable. |
| `motion_shape` | SHPE | Motion shape | MOTION: the tremolo / autopan wave - SINE, TRI or SQR. P-lockable. |
| `rings_mode` | MODE | Rings mode | RINGS: RING (ring modulation: bells, robots) or SHIFT (frequency shift: barber-pole smear). P-lockable. |
| `rings_freq` | FREQ | Rings frequency | RINGS: the modulator frequency, centre 0 out to about 2 kHz; the sign sets the shift direction in SHIFT. P-lockable. |
| `rings_fine` | FINE | Rings fine | RINGS: fine offset of the frequency, +-20 Hz, for slow beating. P-lockable. |
| `rings_feedback` | FBK | Rings feedback | RINGS: feedback around the modulator, off to strong. P-lockable. |
| `limit_gain` | GAIN | Limiter gain | LIMIT: gain into the limiter, 0 to +18 dB. P-lockable. |
| `limit_ceil` | CEIL | Ceiling | LIMIT: the highest output level, -12 to 0 dB. P-lockable. |
| `limit_release` | REL | Limiter release | LIMIT: how fast the gain comes back, 2 to 100 ms. P-lockable. |

### MODULATION (MOD)

MODULATION (LFO): the mod matrix (B1; 8 slots of source, destination and depth per track), the four macros (B2-B5, the overview on B11), two LFOs and velocity, all of them matrix sources. No settings.

| Item | Label | Name | What it does |
|---|---|---|---|
| `lfo1_wave` | WAVE | LFO 1 wave | SINE, TRI, SAW, RSAW (ramp down), SQR or RAND (sample and hold). Not p-lockable. |
| `lfo1_speed` | SPD | LFO 1 speed | One cycle per 8 bars down to 1/64, synced to the tempo. P-lockable. |
| `lfo1_depth` | DEP | LFO 1 depth | LFO 1's level into the mod matrix, -64 to +64; 0 = silent, negative inverts. Each matrix slot with SRC LFO1 scales it by its own DEPTH. P-lockable. |
| `lfo1_mode` | MODE | LFO 1 mode | FREE runs freely; TRIG restarts on steps with LRST on; ONE plays one cycle per trig then stops; HOLD latches its value at each trig. |
| `lfo2_wave` | WAVE | LFO 2 wave | SINE, TRI, SAW, RSAW (ramp down), SQR or RAND (sample and hold). Not p-lockable. |
| `lfo2_speed` | SPD | LFO 2 speed | One cycle per 8 bars down to 1/64, synced to the tempo. P-lockable. |
| `lfo2_depth` | DEP | LFO 2 depth | LFO 2's level into the mod matrix, -64 to +64; 0 = silent, negative inverts. Each matrix slot with SRC LFO2 scales it by its own DEPTH. P-lockable. |
| `lfo2_mode` | MODE | LFO 2 mode | FREE runs freely; TRIG restarts on steps with LRST on; ONE plays one cycle per trig then stops; HOLD latches its value at each trig. |
| `macro1_value` | M1 | Macro 1 | Macro 1's value, 0 to 127: a mod matrix source (M1); the slots with SRC M1 set what it moves and how far. A matrix slot can also aim at it (LFOs and envelopes drive it). P-lockable. |
| `macro2_value` | M2 | Macro 2 | Macro 2's value, 0 to 127: a mod matrix source (M2); the slots with SRC M2 set what it moves and how far. A matrix slot can also aim at it (LFOs and envelopes drive it). P-lockable. |
| `macro3_value` | M3 | Macro 3 | Macro 3's value, 0 to 127: a mod matrix source (M3); the slots with SRC M3 set what it moves and how far. A matrix slot can also aim at it (LFOs and envelopes drive it). P-lockable. |
| `macro4_value` | M4 | Macro 4 | Macro 4's value, 0 to 127: a mod matrix source (M4); the slots with SRC M4 set what it moves and how far. A matrix slot can also aim at it (LFOs and envelopes drive it). P-lockable. |
| `mtx.src` | SRC | Matrix source | The selected row's source: OFF, LFO1, LFO2, ENV (the mod envelope), VEL, AT (aftertouch), MW (mod wheel, CC 1), EXPR (expression pedal, CC 11) or a macro M1-M4. The LFOs are bipolar, the rest unipolar. Not lockable. |
| `mtx.dst` | DST | Matrix destination | What the row moves: any continuous track param or PITCH, grouped by category in category order. A turn steps one item; SHIFT + the knob jumps a whole category. Not lockable. |
| `mtx.depth` | DEPTH | Matrix depth | How far the row's source moves its destination, -100 to +100 % of its range; fine steps near 0, SHIFT snaps to 0 / +-25 / +-50 / +-100. P-lockable and morphed by scenes. |

### MIX (MST)

MIX (EDIT, first): the mixer - every track's level, pan and sends, and the master settings (tempo, swing, global length) when MST is the target. Settings: project name, global BPM, tempo, return mode.

| Item | Label | Name | What it does |
|---|---|---|---|
| `mute` | MUTE | Mute | Silences the track (its sends still feed S1 / S2). Saved with the pattern; also a held track key. |
| `global.glen` | GLEN | Global length | The global pattern length, 1 to 256 steps: the boundary that SEQ pattern changes (and TEMP returns) wait for. |
| `global.rtn` | RTN | Playhead return | What the playhead does when loop keys are let go: RETURN (back in sync), BOOMR (plays back out in reverse) or RESET (restart). |
| `e5.gbpm` | GBPM | Global BPM | ON: one tempo for every pattern. OFF: each pattern keeps its own BPM. |
| `e5.name` | NAME | Project name | Turn to name the project (OCT+ keeps it, OCT- cancels). |

### FILE (FILE)

FILE (the SAVE button, first): the sample browser and the slot's file settings, and RAZOR, the slice editor for the track's sample.

| Item | Label | Name | What it does |
|---|---|---|---|
| `slot.bpm` | BPM | Sample BPM | The tempo the sample was played at, 20 to 300; changing it recalculates its length in steps. Tempo-fitting warp modes use it. |
| `slot.steps` | STEP | Sample steps | How many sixteenth steps the trimmed sample spans, 1 to 512: the length tempo-fitting warp modes stretch it to. |
| `slot.gain` | GAIN | Sample gain | The slot's level, 0 to 4x, times each track's volume. |
| `slot.trim_start` | T-ST | Trim start | Where the sample starts, shown in seconds; the trim is stretched to the slot's STEPS, and everything (Range, loop, slices) lives inside it. SHIFT = finest step. |
| `slot.trim_end` | T-EN | Trim end | Where the sample ends, shown in seconds. SHIFT = finest step. |
| `razor.start` | STRT | Slice start | RAZOR: the selected slice's start, 0 to 1 of the track's window, moved under the SNAP mode. |
| `razor.end` | END | Slice end | RAZOR: the selected slice's end, 0 to 1 of the track's window, moved under the SNAP mode. |
| `razor.play` | PLAY | Slice play mode | RAZOR: the selected slice's own play mode, or TRACK to use the track's. |
| `razor.zoom` | ZOOM | Zoom | RAZOR: zooms the waveform 1x to 64x, centred on the point last moved. View only. |
| `razor.snap` | SNAP | Snap mode | RAZOR: how STRT / END move - GRID (slice-count divisions), ZOOM (fine, by zoom), ZERO-X (zero crossings), TRANS (transients), FRIEND (drags the touching neighbour too). |
| `razor.choke` | CHOK | Slice choke group | RAZOR: the selected slice's choke group, A to H or OFF; a new voice in the group cuts the others. |
| `razor.pitch` | P/T | Slice pitch | RAZOR: the selected slice's pitch offset, -24 to +24 semitones. |
| `razor.sens` | SENS | Transient sensitivity | RAZOR: sensitivity of the transient detection (the ticks) used by Auto-Chop and TRANS snap, 0 to 100. |

### PATTERN LAUNCH (PAT)

PATTERN LAUNCH (SEQ): the track's pattern length and step rate, the global length and how pattern launches engage. Settings: launch mode, global length.

| Item | Label | Name | What it does |
|---|---|---|---|
| `pattern_steps` | LEN | Pattern length | The track's pattern length, 1 to 256 steps. Tracks can differ (polymeter). |
| `global.quant` | QUANT | Pattern change mode | How a picked pattern engages: SEQ (at the global length boundary), JUMP (now, same step), START (now, from step 1), TEMP (now, back after one pass). |

### CROSSFADER (XFD)

CROSSFADER (ARP): the A / B scene crossfader; B1 / B2 are the scenes, the white keys glide the fader. Never sequenced or recorded.

| Item | Label | Name | What it does |
|---|---|---|---|
| `e5.xfd` | XFD | Scene crossfader | The A / B scene fader, 0 (scene A) to 128 (scene B); morphs every param stored in the scenes. |

### OTHER

| Item | Label | Name | What it does |
|---|---|---|---|
| `pattern_rate` | RATE | Pattern rate | The track's step speed against the tempo, 1/16 to 2 and 4 (1 = sixteenth notes; 2 = twice as fast). |

### Machines and modes (what a page variant is for)

**Machines**

- **LOOP**: Plays the loop region continuously; each trig restarts it. Loop keys scrub and lock the region live.
- **TRIG**: The loop engine, but silent except while a triggered step's note lasts; each step can lock its own region.
- **SLICE**: Cuts the region into equal slices (CNT, up to 32) played by trigs and pads. One voice at a time.
- **RAZOR**: Plays the slices of the razor table, each with its own start / end (FILE > RAZOR). One voice at a time.
- **S.POLY**: SLICE with polyphony: up to 8 voices per track, 24 in all (the oldest is stolen).
- **R.POLY**: RAZOR with polyphony: up to 8 voices per track, 24 in all.
- **NEIGH**: No sample: the source is the whole output of the track to the left (T2-T6), heard only through this track's filter, insert, amp and pan.
- **INPUT**: The host's live audio input is the source, through the track's filter, insert, amp and sends; plays whether the transport runs or not.

**Warp modes**

- **TAPE**: Plays at the sample's own speed, like a tape deck: speeding up raises the pitch.
- **VARI**: Tempo varispeed: fits the region to its step length at the current tempo, so it always lands on the loop boundary. P/T has no effect here.
- **DOMINO**: A step-synced slicer: the region is cut into SLCS slices, one played every SLEN steps, each with its own envelope and loop mode.
- **GRAIN**: Granular: overlapping grains read around a moving emitter; size, density, spread and grain speed shape the texture, pitch stays independent.
- **OLA**: Random overlap-add: shuffled grains wander around the playhead; CHAOS blends in the wet grains and widens the wander.
- **PITCH**: Pitch correct: a tempo-fitted read through a pitch shifter, so timing and pitch are set separately (a robotic, formant-y character).
- **HARM**: Harmonizer: the dry loop plus up to three pitch-shifted copies at IN1-IN3 semitones (0 = that voice off).
- **WTBL**: Wavetable: the loop region becomes a bank of single cycles played by an oscillator (pitch from P/T and the keys, base C3), scanned by MORF and its LFO.
- **FREEZE**: Spectral freeze: FRZE captures and holds the spectrum at the playhead as a drone; BLUR smears it.
- **FORMNT**: Formant: shifts the spectral envelope (FORM, in semitones) and can freeze it too.
- **GSTR**: GStretch: pitch-held time stretch; STRCH sets the read speed (1 = real time), a pitch shifter restores the pitch.
- **GSTR2**: GStretch 2: as GSTR, but tempo-synced: the region passes once per loop, STRCH and RATE deviate from that.
- **CHOP**: Chopped (Digitakt-style): a clock ramp sweeps the region and a grain fires every SLEN steps at natural pitch, so the loop stretches to any tempo without changing pitch.

**Filter machines**

- **CLASSC**: Classic multimode filter: low-pass, high-pass, band-pass or notch (TYPE), with cutoff, resonance and drive.
- **MORPH**: Morphing filter: MRPH sweeps continuously from low-pass through notch to high-pass.
- **LADDER**: Moog-style ladder low-pass; rings hard at high resonance but cannot run away.
- **COMB**: Tuned feedback comb: TUNE sets its pitch, FB the resonance, DAMP darkens each echo, MIX blends dry / wet.
- **FORMNT**: Vowel filter bank: VOWL morphs A-E-I-O-U, CUT transposes the formants, RES sharpens them, MIX blends dry / wet.
- **SVF**: State-variable multimode filter (LP / HP / BP / notch) with smoother type changes than CLASSC.

**Filter envelope modes**

- **ADSR**: Attack, decay, sustain, release: the filter envelope follows the note length, sustaining until the note ends.
- **AHR**: Attack, hold, release: the filter envelope ignores the note length; HOLD at its top holds forever.

**Amp envelope modes**

- **ADSR**: Attack, decay, sustain, release: the sound sustains while the step's note lasts (LEN), then releases.
- **AHR**: Attack, hold, release: ignores the note length; HOLD at its top (INF) keeps the sound on until the next trig.

**FX machines**

- **NONE**: No effect: a clean pass-through on an insert or the master; on a send bus nothing is returned.
- **DELAY**: Clean digital tempo-synced delay with a low-pass in the feedback.
- **REVERB**: Algorithmic (FreeVerb-style) reverb.
- **LOFI**: Bit-depth and sample-rate reduction.
- **COMP**: Compressor with real dB / ms readouts and in / gain-reduction meters; MIX below full gives parallel compression.
- **DESTRY**: Destroy: a distortion chain (warble, overdrive, fold, saturation, bitcrush, sample-rate reduction, tone, level). Always fully wet.
- **ECHO**: Coloured stereo echo: tempo-synced time, right-tap offset, stereo or ping-pong, tone in the loop and wobble.
- **B.HOLE**: Blackhole: a huge modulated reverb; GRAV at maximum freezes the tail.
- **CHORUS**: Dual-voice modulated short-delay chorus.
- **FLANGR**: Flanger: a swept short delay with bipolar feedback.
- **PHASER**: Phaser: 1 to 8 swept all-pass stages with feedback.
- **DJ EQ**: Three-band DJ isolator EQ with kills and adjustable crossovers. Always wet.
- **DUCK**: Ducker: dips this slot whenever the dry program plays (best on a send). Always wet.
- **T.ECHO**: Tape echo: tempo-synced delay with wow, flutter and saturation in the loop.
- **CASSET**: Cassette: wow, flutter, hiss, crackle, dropouts and a band-limited tone.
- **MOTION**: Stereo motion: width, plus synced tremolo and autopan. Always wet.
- **RINGS**: Ring modulator or frequency shifter (MODE), with feedback.
- **LIMIT**: Brickwall limiter: push GAIN into the ceiling. Always wet.

### Pages

| Page | What it is for |
|---|---|
| TRIG/TRIG | The basic step settings: note pitch, note length and velocity, and the three reset flags (envelope, LFO, filter). Hold a step to lock any of them on that step. |
| TRIG/COND | Whether and how often a step fires: chance, trig condition (A:B cycles, FILL, PRE, 1ST ...) and ratchet repeats. |
| TRIG/PLAYHEAD | How a trig engages the loop region: trig jump and trig release. On slice machines this page is SLICE instead: slice number, play mode, ratchet, reverse. |
| TRIG/SETUP | The global swing amount, shared by every track. |
| SRC/TRIG | The step's trig settings without leaving LOOP ENGINE (0.6.41): B1 steps through its four lanes: NOTE, LENGTH, VELOCITY, MICRO; SLIDE and the resets; CHANCE, CONDITION, RATCHET; the playhead (slice machines: the slice, its play mode, ratchet, reverse). |
| SRC/LOOP | The main view: the track's waveform with pitch, loop start / end and sample slot (slice machines: play mode, slice, slice count, choke, slice mute). Empty for NEIGH and INPUT, which show their routing / input meters. |
| SRC/MACHINE | The machine's own controls: loop reverse, crossfade and trig release for loop machines; slice, play mode, reverse, ratchet, clock sync and rate for slice machines. |
| SRC/WARP | The warp mode's controls (each mode shows its own: grains, stretch, harmonies, wavetable ...) plus RATE, over the waveform. Empty for NEIGH and INPUT. |
| SRC/RANGE | The Range: a crop of the trimmed, stretched sample that the loop points, loop keys and slices work within. The white keys set it as loop keys, on every machine. |
| SRC/SETUP | The track's machine, warp mode (changes only while stopped), loop-key division and trig polyphony. |
| FILT/FILTER | The filter machine's controls (cutoff, resonance and its own third / fourth knob) and the filter envelope depth, over the filter curve. |
| FILT/FILTER ENV | The filter envelope's times (ADSR or AHR, per the setup) and its depth onto the cutoff. |
| FILT/MIX | How the filter treats the two channels: the stereo / mid-side cutoff spread (BAL) and the dry / wet mix (COMB and FORMNT only). |
| FILT/SETUP | The filter machine, the filter envelope mode and the stereo / mid-side mix mode. Machine and mix mode change only while stopped. |
| AMP/AMP | The track's level, pan, trig velocity and velocity's depth into the mod matrix, and the S1 / S2 send levels. |
| AMP/AMP ENV | The amp envelope (ADSR or AHR) over the envelope's shape. |
| AMP/MOD ENV | The mod envelope (ADSR) and its depth, its level into the mod matrix (MOD/MATRIX sets where it goes). It retriggers on steps whose ERST is on, for the note's length. |
| AMP/SETUP | Amp envelope mode, the time range of every envelope, portamento (legato), and how long slice notes last. |
| FX/SENDS | How much of the track goes to the S1 and S2 send buses, and where it is tapped (before or after the insert). |
| FX/MIX | The insert's dry / wet mix and drive, and the S1 / S2 send levels: B11 in FX. |
| FX/SETUP | Choose the insert FX machine (any time, with a short fade) and the send tap point. |
| MOD/MATRIX | The mod matrix: 8 slots per track, one row each, SRC · DST · DEPTH. SELECT picks the row (past row 8 it goes on to the next page); KNOB 1 the source, KNOB 2 the destination, KNOB 3 the depth. Fixed: its knobs are always these. Hold MOD (or SHIFT + MOD) for MOD KEYS: hold a black key (a source) and turn any knob to aim it there. |
| MOD/LFO 1 | LFO 1: waveform, synced speed, depth and trigger mode. Where it goes is set in the mod matrix (MOD/MATRIX); the depth is its level into the matrix. |
| MOD/LFO 2 | LFO 2: waveform, synced speed, depth and trigger mode. Where it goes is set in the mod matrix (MOD/MATRIX); the depth is its level into the matrix. |
| MOD/VELOCITY | Velocity as a mod source: its depth is its level into the mod matrix (MOD/MATRIX sets where it goes). Velocity comes from slice trigs only. |
| MOD/MACROS | The four macro values M1-M4 at a glance. Each macro is a mod matrix source (M1-M4): MOD/MATRIX sets what it moves. |
| MOD/MACRO 1 | Macro 1: its value; the picture lists the mod matrix rows whose source is M1 (read-only: edit them on MOD/MATRIX). |
| MOD/MACRO 2 | Macro 2: its value; the picture lists the mod matrix rows whose source is M2 (read-only: edit them on MOD/MATRIX). |
| MOD/MACRO 3 | Macro 3: its value; the picture lists the mod matrix rows whose source is M3 (read-only: edit them on MOD/MATRIX). |
| MOD/MACRO 4 | Macro 4: its value; the picture lists the mod matrix rows whose source is M4 (read-only: edit them on MOD/MATRIX). |
| MST/TRACK MIX | The mixer: all six tracks' levels and the S1 / S2 returns at a glance; the knobs set the selected track's level, pan and two sends, and lane 2 its mute. |
| MST/MASTER | Project timing: tempo, swing, global pattern length and what the playhead does when the loop keys are let go. |
| MST/SETUP | Playhead return mode, tempo, global BPM (one tempo for every pattern) and the project's name. |
| PAT/PATTERN | The pattern grid with the track's pattern length and step rate, the global length and the pattern change mode. |
| PAT/SETUP | How a newly picked pattern engages (quantize mode) and the global length it waits for. |
| XFD/SCENE | The A / B scene crossfader: the fader, its glide positions and the params that morph. The lower keys are the scene row here. |
| bus:S1 | Send bus 1's effect: the knobs of the FX machine on the S1 return (global, not p-lockable; morphs with the scenes). |
| bus:S1/SETUP | Choose the FX machine on the S1 return. NONE returns nothing. |
| bus:S2 | Send bus 2's effect: the knobs of the FX machine on the S2 return (global, not p-lockable; morphs with the scenes). |
| bus:S2/SETUP | Choose the FX machine on the S2 return. NONE returns nothing. |
| bus:MST | The master insert: the knobs of the FX machine on the final mix, send returns included (global, not p-lockable). |
| bus:MST/SETUP | Choose the FX machine on the master insert. NONE passes the mix through clean. |
| bus:MST/SRC | With the master bus selected, SRC shows the mixer: track levels and the returns, with the selected track's volume, pan, mute and the tempo. |
| F3 | The timing pop-up (OCT- + OCT+, or PLAY held): tempo on SELECT, swing on ALGORITHM, the track's pattern length and rate, global length and change mode; the lower keys pick and loop the step pages. |

<!-- params:end -->

## The modes

The page tables above give each mode's pages and knobs. This part gives the keys.

### FX (FX)

| Key | Does |
|---|---|
| B1 / B2 | **INSERT 1 / INSERT 2** (EFFECTS, that row); tapped again: its knob pages. Hold: its type menu (SENDS: raise its box; K1 moves it) |
| B3 / B4 | **SEND 1 / SEND 2** (EFFECTS, that row: the target becomes the bus); tapped again: its knob pages. Hold: its type menu (SENDS: raise its box; K1 moves it) |
| B5 | **SENDS**, then **MASTER** P1 .. (the master effect, the load meter), then SENDS again. Hold on MASTER: its type menu |
| ALGO | on SENDS: the send mode PRE / POST |
| B6-B10 | the sources |
| B11 | the **MIX** page (FX MIX, DRIVE, SEND 1, SEND 2) |
| White keys | steps |

An FX machine changes at any time (a short fade, no click).

### FILTER (SEL)

- Pages: FILTER (B1), FILTER ENV (B2), MIX (B3).
- **Hold B1 = the filter type**, with K1 or SEL.
- **B11 = the filter envelope's mode**, ADSR ↔ AHR (its label says which).
- B6-B10 the sources. White keys: steps.

### AMP (ENV)

- Pages: AMP (B1), AMP ENV (B2), MOD ENV (B3).
- **Hold B1 = the envelope mode**, with K1 or SEL.
- B6-B10 the sources. B11 unused. White keys: steps.

### MODULATION (LFO)

| Key | Does |
|---|---|
| B1 | the **MATRIX** |
| B2-B5 | **M1-M4**, the macros. A tap opens that macro's page (MODULATION stays). Hold + turn a knob routes that macro, like the other sources (header M1..M4 on SRC) |
| B6-B10 | the sources (LFO 1, LFO 2 and VEL taps open their pages here, AT the MATRIX) |
| B11 | the **MACROS** overview (M1-M4 on K1-K4) |

- The macro values are params: they lock on steps and live REC records them.
- **The MATRIX page:** SEL picks the row (on into the next page after row 8), K1 SRC, K2 DST (one a click; SHIFT +
  K2 jumps a category), K3 DEPTH. The sources are LFO1, LFO2, ENV (the mod envelope), VEL, AT (aftertouch, the
  highest held key on the selected track), MW (MIDI CC 1), EXPR (CC 11) and M1-M4. MW and EXPR are set here only.
- A value aimed at by a slot shows a small mark and the source's tag in its cell, on every page.

### MIX (EDIT, first)

| Key | Does |
|---|---|
| W1-W9 | the targets T1-T6, S1, S2, MST. **Tap = select. Hold = mute / unmute** (T1-T6; S1, S2, MST: a hold selects) |
| SHIFT + W1-W6 | **solo** / unsolo |
| SHIFT + B11 (FILL) held + W1-W6 | every mute and solo off |
| B1-B8 | the selected track's **rate**: 1/8, 1/4, 1/2, 3/4, 1X, 3/2, 2X, 4X |
| B9 / B10 | **slower / faster** (the next rate in the list) |
| B11 | **the CAT** on / off |

- The knobs show TRACK MIX (LEVEL PAN SEND 1 SEND 2 / MUTE) for a track, MASTER (BPM, SWING, GLO LENGTH) for MST.
- **The CAT** is the norns visualizer: the dancing cat, full screen, its speed following the tempo, in front of a
  greyscale fractal (0.6.3: a different algorithm each time it opens, never the same twice running: Julia, Mandelbrot
  zooms, Burning Ship, Tricorn, a quartic Julia, Phoenix, Newton; always evolving, faster while playing, pulsing on
  each beat and harder on the bar). B11 or
  any mode button leaves it (EDIT: back to MIX). The track keys keep working while it shows; the knobs don't.
- Mutes are the pattern's (saved); solos are the session's. When any track is soloed only soloed ones play.
- LEDs: W1-W9 the selected one bright, the others by their level, muted (or silenced by a solo) off, soloed
  blinking. B1-B8 the current rate bright.

### PAGES (EDIT, second)

- **The black keys are the pattern's step pages** (FIZZ style). **A tap edits the page** (the white keys follow).
  **A hold puts it in or out of the playing loop** (never empty).
- **F1 / F2 on:** the black keys copy / cut and paste **whole pages** (as steps, above).
- White keys: the edited page's steps.
- Knobs: LENGTH, RATE, LOOP (the pages in the loop, "1-2").
- LEDs: the edited page bright, the loop's pages mid, the others faint, the playing page blinks.

### LOOP ENGINE (HOME, first)

| Key | Does |
|---|---|
| B1 | 0.6.41: **TRIG**, the step's trig settings: a tap steps its four lanes (NOTE LEN VEL MICRO; SLIDE and the resets; CHANCE COND RATCHET; the playhead, slice machines the slice's) (hold: ADVANCE) |
| B2 | **LOOP** (hold: the loop / slice play mode) |
| B3 / B4 / B5 | **MACHINE**, **WARP**, **RANGE** (hold: the machine, the warp mode, the voice mode) |
| B6-B10 | the sources |
| PRESETS | **the step page** (steps 1-16, 17-32 ..; 0.6.40: the header's second box, S1 S2 .., beside the track) |
| ALGO | **selects one step** (shared with KEYBOARD's target; 0.6.11: each track's own); below step 1 is **LIVE** (nothing selected) |
| B11 held + B1..B8 | 0.6.13: **the pages**, as FIZZ: tap = select, hold = mute (out of the loop), double-tap = solo, two held = loop between them (past the end: the track grows to it). The screen's cells become the pages panel; **K1 = LEN, K2 = RATE, K3 = the zoom** (0.6.14); 0.6.40: **ALGO = LEN** too, **PRESETS = the zoom** too (slice tracks: the pads' bank, SLICES 1-16 / 17-32, LOOP DIV untouched) |
| B11 | **the STEP layer**: held = the white keys select steps; a quick click latches it (another quick click: loop keys again). Latched, held = the loop keys. B11 never goes LIVE. Held + **OCT-**: clear the selected steps. Held + K3 or PRESETS: the zoom (16 loop keys → 32, keys 1-16 → 32, keys 17-32; slice tracks: slices 1-16 / 17-32); held + K1 or ALGO: the length |
| White keys | **the loop keys, always live**; with steps selected each entry is also written into them. STEP layer: select steps |

- **Sequencing regions and ranges (0.6.9)**: **hold B11** and press step keys to select steps (as FIZZ: keys held
  together are one selection; once you let go of them, the next press starts a new one), then let go of B11: the white keys are loop keys again, and **they
  play exactly as live**, but each entry (the keys held together) also goes into **every selected step**, written
  at every press (0.6.24) and once more on the last release, which shows the message and closes the undo gesture:
  its region lock (one key: that division; several: first..last) and a trig. On the
  **RANGE page** (B5; B4 before 0.6.41) the entry is the steps' **range** (the 16 keys over the whole sample). The selection stays
  until you select again (holding B11 and pressing nothing keeps it); 0.6.11: it is **the track's own** (T2 has its
  own selection and target step; switching tracks never writes into another track's steps), and on the LOOP layer the
  selected steps stay in view as **bright caps over the loop-key ruler**; ALGO selects one step, and ALGO below step 1
  is LIVE (nothing selected). **A quick click on B11 latches the STEP layer** (press steps to select them; hold B11
  for the loop keys, the reverse order); another quick click goes back to loop keys. **B11 + OCT-** clears the
  selected steps (trig and locks; SHIFT + F1 undoes). With steps selected **the knobs lock onto all of them** (the
  cells show the first one's; F1 + a knob clears their lock). **On the loop keys everything is as live** (0.6.10):
  the ruler, the brackets and the bright span follow every press; since the entry is written into the selected step
  at every press, the brackets, the span and the key LEDs show that step's lock, drawn in the step's own range window
  (0.6.25 / 0.6.26), keys held or not (the selected steps still blink). The STEP layer shows the step strip (the selected steps capped, each step's lock as a
  small bar), its keys light the selected steps (trigs mid), and the header says SELECT STEPS.
- **The loop keys** (the live scrub): hold one key to loop that part of the range, two or more to span them.
  0.6.21 / 0.6.23: **a press refits the span to the keys held now**, smaller or larger; **a release keeps it** (let
  one of two go and the loop stays while the other is held); **pressing a held key's direct neighbour again**, when
  the span is just those two keys, drops back to the held key alone (W1 held: W2 = 1-2, W2 again = 1, again = 1-2).
  LIVE, every press writes the track's region (RANGE page: its range), so the cells, markers and LEDs follow at once;
  the header's message comes on the last release. Let go of all of them and the playhead returns as RET MODE says
  (return / boomerang / reset). While stopped they audition; **stopped with a step selected, they (and the slice
  pads) play through that step's region / range locks** (0.6.30), as the screen shows.
- **The RANGE page (B5) is the same on every machine** (0.6.21-0.6.28): the white keys are loop keys editing R-START
  / R-END over the 16 keys, with the same span rules, LEDs and markers (on slice tracks too: no pads there, and the
  page is drawn by the loop track's code). The range is **a crop, never a repitch**: the trim is stretched to the
  slot's STEPS, the range crops that, and the loop points or slices work in 0..128 of the range, so a sequence of
  chops keeps playing while the range moves under it.
- **On slice machines** (SLICE, S.POLY, RAZOR, R.POLY) the white keys are **slice pads**, velocity sensitive (RANGE
  page excepted). The LOOP ENGINE / TRIG waveform draws the slices as the SLICE editor does (0.6.18): edges dark
  behind the wave, a sounding slice a cream block, muted slices dim; all COUNT slices always span the view's range
  (0.6.29). The pages are named after the machine (0.6.19: SLICE/SLICE, RAZOR/MACHINE ..). With the amp release
  at INF a slice voice still ends: a 20 ms release when a 1SHOT reaches its end or a HOLD lets go (0.6.32).
- **The waveform shows only the range (0.6.8)**: on LOOP, MACHINE and WARP the view is range start..end (the
  target step's range lock when it has one), so changing the range trims what you see; the RANGE page shows the
  whole sample with the range bracketed. **With a target step the brackets are that step's region** (its lock, in
  the lock colour; else the track's), so scrolling ALGO through the steps moves them. 0.6.27: **LIVE with the
  transport running and no loop key held the brackets are hidden**; the bright span still shows what plays. LIVE,
  the RANGE page shows the resolved range (the playing step's lock).
- The screen shows the waveform; with the GRAIN warp each live grain is drawn on it (a block at its read position,
  as wide as the grain, shaded by its level). The other warps show the playhead.
- On a bus the white keys do nothing.
- LEDs: the loop's keys dim, the playhead's key mid, held keys bright. Slice pads (0.6.37 / 0.6.39): the keys with a slice
  (1..COUNT) dim, the pads held, LIVE the slices sounding, and with a step selected the slices that step plays bright (the waveform shows that
  step's slices as cream blocks too, so ALGO walks through them). 0.6.25: LIVE the loop keys light **what plays now** (each step's region as it fires, the held keys'
  span); the RANGE page lights the resolved range.

### TRIG (HOME, second)

- Pages: TRIG (B1), COND (B2), PLAYHEAD (B3; on slice machines its slice settings).
- B6-B10 the sources. B11 unused. White keys: steps.
- TRIG's K4 is **MICRO**, the step's timing offset in 1/24 steps (what live REC writes).

### KEYBOARD (HOME, third)

- **The keybed is a keyboard** for the selected track: the white keys and the black keys are the notes, C4 (the
  sample's own pitch) on W5.
- **F1 = octave down, F2 = octave up** (-3..+3). There is no copy / cut here.
- **On slice tracks** (SLICE, S.POLY, RAZOR, R.POLY): **B1-B10 pick the slice** (ten at a time) and play it, **B11
  steps the slice bank** (1-10, 11-20 ...). The white keys play the selected slice at a pitch.
- **Stopped, a key plays the sound while it is held**: the loop from its start (on slice tracks the selected
  slice) at that key's pitch; letting go stops it. Playing, a key sets the track's pitch as before.
- **The key lit is the note sounding**: the key held plus the track's PITCH, so a positive PITCH lights a key higher
  up the keyboard, a negative one lower (and turning PITCH while holding a key moves it).
- **ALGO = the target step** (no pop-up, no message: the step strip shows it). Below step 1 is **LIVE** (no
  entry). With a target step, a key writes a trig with that note (pitch and velocity locked; on slice tracks the slice) on the step, then moves on to the next step.
  - **ADVANCE** (in HOME's settings, default on) says whether it moves on. **SHIFT + the key** does the other.
  - One note per step: a new note replaces the step's.
- Live (no target), REC records what you play.
- Knobs: OCTAVE (or SLICE), PITCH, VEL, STEP.
- LEDs: the white keys faint, the Cs dim; slice keys dim, the selected slice bright. F1 / F2 lit when the octave is
  below / above 0.

### FILE (FILE, first)

- 0.6.11: **FILE opens while playing** (BROWSE, POOL, the SAMPLER; loads and previews too). Only the **EDITOR and
  RAZOR** (B4 / B5), **FREE**, **MOVE** and **purge** say STOP FIRST. Inside FILE, PLAY starts and stops (from the
  EDITOR / RAZOR it goes to BROWSE first: "BROWSE: PLAY AGAIN"); REC does nothing.
- Pages: **BROWSE (B3)** and **POOL (B6)**. 0.6.15: **the FILE button cycles BROWSE (SAMPLES) > PROJECTS (0.6.42) > EDITOR >
  SLICE (RAZOR) > SAMPLER** (playing, the editors are skipped); FILE held: the five on B1-B5; the editors have no page keys, so their black keys are all theirs (the zoom).
- **SEL turns the page on EDITOR and RAZOR**, both ways (lane by lane: EDITOR and RAZOR have two lanes each, then the
  next page): BROWSE < EDITOR < RAZOR < POOL. On **BROWSE and POOL, SEL is the list's cursor**; their way out is a page
  key (B3 B4 B5 B6). The EDITOR's black keys are all zoom keys, so **SEL** leaves it (back to BROWSE, on to RAZOR).
  Tapping FILE goes on to the SAMPLER, as always.

**BROWSE**

| Control | Does |
|---|---|
| SEL | the cursor: the folder's subfolders first, then its files |
| B1 / B2 | **up a folder / into the folder** under the cursor. 0.6.42: **AUDIO/ is the top** (nothing above it; the projects are PROJECTS'); below it the first row is **UP** (an arrow, the folder it goes to on the right) |
| OCT+ / OCT- | 0.6.42: **open** the row under the cursor (UP: up, a folder: into it; 0.6.44: a file: **in or out of the pool**) / **up** |
| B10 | **LOAD**: the file goes into **the next free pool slot** (0.6.32: the pool only: a track plays it once its SLOT is set, with K2 on the LOOP page, PRESETS or POOL's ASSIGN). The cursor stays put. A file already in the pool isn't loaded twice (IN SLOT n). A full pool says FULL |
| B11 | 0.6.44 (owner): **a click: the file in / out of the pool** (as OCT+); **held: the file previews until let go** (AUTO-PREVIEW off, PRESETS: hold B11 to hear, click to take it) |
| B9 | **free the pool slot** of the file under the cursor (its tracks fall silent; 0.6.42: was F2) |
| SHIFT + B11 | **purge**: free every slot no pattern uses and compact the memory (not while playing) |

- Every cursor move **previews** the file (APP SETTINGS > AUTO-PREVIEW). The preview plays on the main output and
  stops when you leave FILE. LOAD of the previewed file reuses its memory (no second load).
- Knobs: SLOT (the one LOAD fills), LENGTH, BPM, FREE (MiB). **The pool meter** shows the memory used / free and the
  slots used of 128.

**EDITOR** (the selected track's sample)

| Control | Does |
|---|---|
| B2-B10 | the zoom: 1X, 2X, 4X .. 256X |
| B1 / B11 | scroll left / right half a view (when zoomed) |
| F1 / F2 | **START / END**: the point the white keys place (the header's right side says which; no copy / cut here) |
| White keys | **16 places across the view** (the zoom sets how fine): a tap places the chosen point there |
| A white key held + a second | **both points**: START: the held key is the start, the second the end; END: the reverse |
| SEL | the editor's pages (P1 START / END / ZOOM, P2 BPM / STEPS / GAIN / ZOOM); it stops at the ends. FILE leaves (SLICE next) |
| K1-K4 | START, END, BPM, GAIN / STEPS |

START never passes END: a placement that would put START after END swaps them. Each placement is one undo (SHIFT +
F1); a two-key gesture is one. The knobs' START / END are undoable too (0.6.16: START, END, AUTO BPM and RECALC each a
gesture of its own). The steps stay until B10 RECALC (0.6.15). The chosen point's
marker is ACC, the other light grey. LEDs: the keys at the trim points lit.

**SLICE** (the slice editor, FILE/SLICE; 0.4's RAZOR, reworked in 0.6.15)

| Control | Does |
|---|---|
| ALGO | **select the slice** (0.6.1; it was SEL) |
| SEL | the editor's pages P1-P3; it stops at the ends. FILE leaves (the SAMPLER next) |
| K1-K4 | P1: SLCE START, SLCE END, PLAY, ZOOM / P2: SNAP MODE, CHOKE, PITCH, ZOOM / P3: COUNT, TRANS SENS, -, ZOOM |
| B1 / B2 / B3 | the pages |
| B9 / B10 | **EVEN** (COUNT equal slices) / **CHOP** (one slice per transient at TRANS SENS; 0.6.16: slice 1 always starts at 0) |
| B11 held | the selected slice heard, looping, until let go |
| F2 / F1 | **split** the selected slice / **delete** it |
| White keys | the slices of the selected slice's bank of 16 (slice machines: the pad; any other machine: a preview); SHIFT + one mutes the slice |

The razor machines (RAZOR, R.POLY) play each slice from its own start / end. The table is per track and saved with
the project. Split and delete renumber the slices after it; the track's steps follow. 0.6.16: every table edit
(EVEN, CHOP, split, delete, mute, the slice knobs) and the COUNT knob are on the undo stack, as one entry each
(a knob's turns coalesce); undoing COUNT brings the table back with it.

**POOL** (all 128 slots, 0.6.1)

| Control | Does |
|---|---|
| SEL | the slot (empty ones included). Moving onto a used slot **previews** it (APP SETTINGS > AUTO-PREVIEW) |
| B10 | **ASSIGN**: the selected track plays this slot |
| B11 | **preview** play / stop |
| F1 | **MOVE**: picks the slot up (the header says MOVE n); SEL to another slot; F1 again drops it there, **swapping** with what is there. Every reference follows: the tracks' slots and the slot step locks (and slice locks) in all 64 patterns. One undo. F1 on the slot it came from puts it back (NOT MOVED); leaving POOL drops the move |
| F2 | **FREE** the slot (its tracks fall silent), as in BROWSE |
| SHIFT + B11 | **purge**, as in BROWSE |
| B3 / B4 / B5 | BROWSE / EDITOR / RAZOR |

- A row: the slot's number, its file (or a dash), its length, and six marks T1-T6: lit for the tracks of the playing
  pattern on that slot (the selected track's in ACC). A slot only step locks play shows a LOCK bar instead.
- The header is the pool meter: POOL n/128 (slots used) and the memory used in %, with its bar under the header.
- POOL opens while playing (0.6.11); only FREE, MOVE and purge need a stop. Memorized projects (SHIFT + B5) keep the pool as it was.

**REC held in the SAMPLER: the recorder's setup** (0.4's REC pop-up; 0.6.15). It samples the live input or the master out into a pool
slot. Its settings are a list (0.6.1: SEL the row, K1 its value): SOURCE (IN / MASTER), LENGTH, THRESHOLD (with SYNC
FREE), SYNC (PLAY / BAR / FREE), SLOT (the target pool slot). While a take is armed or running only THRESHOLD changes.
**REC or F2** = arm, then record now, then stop; **F1** = close, disarm, or discard the take. HOME closes it. The
take is saved as a WAV in `data/elasticat/rec/`.

### SAMPLER (FILE, second)

It opens while playing too.

| Control | Does |
|---|---|
| B11 | **arm**. Recording starts on the next bar (at once when stopped). B11 again: DISARM when armed; while recording it **stops at the end of the bar** |
| B1-B5 | the length: **FREE, 1, 2, 4, 8 bars** (a set length ends by itself) |
| SHIFT + B11 | purge (as in FILE) |
| White keys | **the input level meter** (-48 .. 0 dB) |
| K1-K4 | LENGTH, GAIN (the input gain), MONITOR (hear the input), FREE |

The take lands in **the next free pool slot**, tagged with the BPM, and is saved to `/data/rec`. The source is the
input (the recorder's setup, REC held, can pick the master out; its SLOT row is the selected track's slot: a resample
replaces the track's sample, so pick another slot there to keep it). FULL when the pool has no free slot.

### CROSSFADER (ARP)

| Control | Does |
|---|---|
| B1 / B2 | **scene A / scene B**. **Hold one + turn a knob** = put that param's value (the selected track) into that scene. The knobs show the page of the mode you came from |
| B11 held + B1 / B2 | **capture**: every morphable param of every track and bus into that scene. The header says CAPTURE while B11 is held |
| SHIFT + B1 / B2 | **clear** that scene |
| W1-W12 | **the fader**, W1 = 0 .. W12 = 127. **Hold a key** and the fader glides there. **More pressure = faster.** Let go and it stays where it got to |
| SHIFT + W1-W12 | **jump** there at once |
| W13-W16 | the **base glide time**: instant / fast / medium / slow (the time with no pressure; saved with APP SETTINGS) |
| K1 | the fader as a knob |

- Knobs: POSITION, GLIDE, A LOCKS, B LOCKS (the number of values in each scene).
- **The crossfader is never sequenced or recorded.** In CROSSFADER, key pressure is the glide's, not aftertouch.
- Morphing: a firing step's lock wins, then the fader, then the track value. The track value is never written. Only
  continuous params morph. The scenes and the fader are saved with the project (one pair for all patterns).
- LEDs: B1 / B2 bright while held, mid when the scene has values; W1-W12 around the fader's position, the held key
  bright; the glide time's key lit.

### PATTERN LAUNCH (SEQ)

| Control | Does |
|---|---|
| B1-B4 | the **bank** A-D (SEL too) |
| White keys | **launch** the bank's 16 patterns, with the launch quantize (F3's LNCH MODE). Stopped: at once |
| F1 / F2 on + white keys | copy / cut and paste **patterns** |
| Hold SEQ / SHIFT + SEQ | F3 (which names the pattern) |

- Knobs: PLAYING, NEXT, QUANT, LENGTH.
- LEDs: the playing pattern bright, the queued one blinking, used ones dim, empty ones faint.

## Live recording (REC)

**REC toggles recording in every mode but FILE.** While the transport runs, what you play is written on the
selected track:

| What | Written as |
|---|---|
| KEYBOARD's notes (LIVE, no target step) | a trig with the pitch and velocity locked; on slice tracks the slice |
| LOOP ENGINE's keys | a trig with that key's loop position locked; on slice tracks the pad's slice (poly machines stack) |
| knob moves (AUTOMATION on) | a lock of that param on the step, without a trig where there is none; the macro values too |
| the crossfader | never |

- **Timing:** each hit goes to the nearest step and keeps its early / late offset as a MICRO lock (QUANTIZE KEEP).
  QUANTIZE HARD drops the offset. An early hit's step doesn't fire again on that pass.
- Nothing is cleared: a recorded step replaces only what it writes. Undo takes it back.
- **PLAY stopping the transport ends recording.**

**The record settings** (hold REC, or SHIFT + REC; saved with the project). SEL the row, K1-K4 the values, F2 or REC
OK, F1 back, HOME closes.

| Row | Choices |
|---|---|
| MODE | **CAPTURE** (stopped: the first key or loop key starts playback, no count-in; then as OVERDUB), **FIXED** (stops recording after LENGTH; playback goes on), **OVERDUB** (adds and replaces) |
| LENGTH | 1 / 2 / 4 / 8 bars (FIXED) |
| COUNT-IN | OFF / 1 / 2 bars (FIXED and OVERDUB, started from stop) |
| METRONOME | OFF / REC (while recording) / ALWAYS |
| CLICK LVL | OFF, -36 .. 0 dB |
| AUTOMATION | ON / OFF |
| QUANTIZE | KEEP (the offset) / HARD |

- FIXED and OVERDUB started from stop start playback (after the count-in). FIXED started while playing records
  LENGTH bars from then.
- **The metronome and count-in click** play on the main output **after the master bus**, so they are never
  resampled or in a SAMPLER take. The count-in clicks even with the metronome off. The sound is APP SETTINGS > METRO
  SOUND.

## The sample pool

- **128 slots.** A track plays its slot (PRESETS); a step can lock another slot (hold the step + PRESETS).
- **SLOT OFF is the default** (0.6.32): a track plays nothing until you set its slot (K2 on the LOOP page, PRESETS'
  pool walk, POOL's ASSIGN). A new project's T1 starts on slot 1 (power-on loads the folder's first file there);
  T2-T6 are OFF. Loading a file only fills the pool. The cells read OFF, the sample area SLOT OFF. Projects saved
  before 0.6.32 keep the slots they had.
- **No per-sample length limit.** The samples share one memory arena of 200 MiB (about 19.8 minutes of stereo at
  44.1 kHz; 0.6.1 keeps 11 MiB for packing the project file).
- Fill it from FILE (B10 LOAD, the next free slot) or the SAMPLER. Free slots with F2 in FILE's browser or POOL, or
  SHIFT + B11 (purge) for every slot no pattern uses. FILE / POOL lists every slot, assigns one to the track and moves
  slots (references follow).
- The use shows in FILE's pool meter, the SAMPLER's FREE, and PRESETS' takeover.
- Older projects load into slots 1-16.

## Projects (0.6.1)

- **A project is a file**, /data/projects/PROJ-001.ecp .. in the cart's own folder; its size has no limit (the
  largest possible project is about 11 MB). **SHIFT + FILE saves** over the current project's file; the project
  also saves itself when stopped, 2 s after the last edit. A new project gets the next free name (PROJ-001,
  PROJ-002 ..) at its first save.
- Saving never stops the sound: the header says **SAVING** until the file has landed, then SAVED. A failed write
  says why in the header (NOT SAVED: DISK FULL ..), and the last whole copy of the file stays (a hidden
  .PROJ-001.ecp is written first).
- **Power-on loads the last project** (its name is kept in the cart's flash image, beside APP SETTINGS, and since
  0.6.31 also in the folder, data/elasticat/last.txt, written at every save and load, so a fresh flash after a
  firmware update comes back to it). A project
  saved by 0.6.0 or older (in the flash) loads once; the next save makes its file.
- Open another project in **PROJECTS** (0.6.42: FILE tapped twice, or FILE held + B2):

| Control | Does |
|---|---|
| SEL / ALGO | the cursor: every project in /data/projects **by name** (0.6.45: the numbers in order: SPICY WALRUS, SPICY WALRUS 2 .. SPICY WALRUS 10); the loaded one says OPEN |
| OCT+ or B11 | **LOAD** the project under the cursor (0.6.46: B11, as SAMPLES; it stays in PROJECTS, the cat dancing a moment over the card: the user drives mode changes); with unsaved edits it **asks** (LOAD X? in the header, the back / OK arrows): **OCT+ yes** (the edits saved, then it opens), **OCT- no** |
| OCT- | back to SAMPLES (a question waiting: no) |
| B10 | **SAVE** the current project (as SHIFT + FILE anywhere); 0.6.45: with **another project under the cursor it asks** (OVERWRITE X?): yes writes this project over that file, which is the project from then on (SAVED OVER IT) |
| B9 | **SAVE AS** (+1): the current project saved as a new file, its name plus the next free number (PROJ-001 2), which is now the project; the old file stays; 0.6.45: the cursor goes to the copy, the header says SAVED +1 |
| B1 | **NEW**: asks (NEW PROJECT?); yes: the edits saved, an empty project (the folder's first file on T1) under the next free name; it stays in PROJECTS |
| B6 | 0.6.45 (0.6.46: B6): **DELETE** the project under the cursor: asked (DELETE X?, GONE FOR GOOD); the open project deleted stays open, unsaved (EDITED; its next save makes a new file) |
| B2 | 0.6.47: **RENAME** the project under the cursor: the name keybed (above, Naming); OK renames its file (the open project: saved under the new name, the old file deleted) |

  The top line: the project you work on and EDITED / SAVED. The lower half: the project under the cursor, its name
  big, its 64 patterns as blocks (written cream, the one it opens on in the accent), its tempo and samples.

## APP SETTINGS (SHIFT + HOME)

Everything global. They are saved in their own record of the cart's flash image, not in the project, so they
survive project loads and new projects.

| Row | Choices |
|---|---|
| THEME | CREAM (default), AMBER, PHOSPHOR, DMG, ICE, PAPER. The theme changes as you scroll |
| KNOB SPEED | SLOW / MED / FAST |
| AUTO-PREVIEW | ON / OFF (FILE's browser) |
| AFTERTOUCH | ON / OFF |
| METRO SOUND | WOOD / BEEP / CLICK / COWBELL |
| MIDI CC | OFF / 1 / 11 / 1+11 (the MW and EXPR inputs) |
| INTRO | ON / OFF |

- SEL the row; K1-K4 change the four rows of the cursor's group.
- **F1 = back** (every change undone), **F2 = OK**, HOME closes and keeps.
- The same record holds KEYBOARD's ADVANCE (HOME's settings) and CROSSFADER's glide time (W13-W16).
- 0.6.1 dropped the MODE POP-UP row; a 0.6.0 record still loads with all its other values.


## Machines, warp modes and FX

- **Machines** (hold HOME: MACHINE, or hold B1 in LOOP ENGINE): **LOOP**, **TRIG**, **SLICE** / **S.POLY** (even
  slices), **RAZOR** / **R.POLY** (the razor table's slices, mono / up to 8 voices), **NEIGH** (T2-T6: the track to
  its left, whole, after its filter, amp, insert and pan; chains work), **INPUT** (the host's live audio input; the
  track is the monitor; NO INPUT when the host has none).
- **Warp modes** (hold HOME: WARP MODE, while stopped): TAPE, VARI, DOMINO, GRAIN, OLA, PITCH, HARM, WTBL, FREEZE,
  FORMNT, GSTR, GSTR2, CHOP. Their params are on LOOP ENGINE's WARP page (the tables above). The spectral modes are
  limited to a few tracks at a time.
- **18 FX machines** (NONE, DELAY, REVERB, LOFI, COMP, DESTROY, ECHO, BLACKHOLE, CHORUS, FLANGER, PHASER, DJ EQ, DUCK,
  TAPE ECHO, CASSETTE, MOTION, RINGS, LIMIT) on each track's insert, the S1 and S2 returns and the master insert. On
  a send, NONE returns nothing. A muted track still feeds its sends. DUCK listens to the dry program.
- A stopped track that has gone quiet for 4 s stops using CPU until it plays again.

## What moved in 0.6.0

| 0.5.0 | 0.6.0 |
|---|---|
| The lower-key modes from the OCT keys (STEPS / LOOP KEYS / KEYS / SCENE) | the modes LOOP ENGINE, KEYBOARD, CROSSFADER; steps are the default |
| SEL held = the loop keys | LOOP ENGINE's white keys |
| MOD KEYS (hold LFO) | the source template on the black keys, always there |
| The track keys on the black row (T1-T6, S1, S2) | SHIFT + W1-W9, SHIFT + PRESETS, and MIX |
| The MST key and category | MIX (EDIT); MST is SHIFT + W9 or MIX's W9 |
| PATN (the pattern overlay) | PATTERN LAUNCH (SEQ) |
| The FILL key | SHIFT + B11 |
| SHIFT + OCT- / OCT+ = undo / redo | SHIFT + F1 / F2 (the same buttons) |
| OCT+ held + a knob = clear a lock | F1 + a knob (F2 + a knob clears modulation) |
| A pop-up's OCT- = back, OCT+ = OK | stays: F1 = back, F2 = OK, inside pop-ups only |
| ALGO = the track | ALGO = the step page; SHIFT + PRESETS = the track |
| PRESETS = the next file of the folder | PRESETS = the track's pool slot (used slots only) |
| SHIFT + PRESETS = the pattern | PATTERN LAUNCH |
| HOME = back to the main view | HOME = LOOP ENGINE → TRIG → KEYBOARD; every mode is a main view |
| SEQ = TRIG, SEL = SRC, EDIT = FILT, ARP = PAT | TRIG is HOME's second sub-mode, SRC is LOOP ENGINE (HOME), FILTER is on SEL, PATTERN LAUNCH on SEQ |
| PLAY on release, PLAY held = F3 | PLAY on the press; F3 = F1 + F2, or hold SEQ / SHIFT + SEQ |
| SHIFT + PLAY = stop + step 1 | PLAY stops (the next start is from step 1) |
| SHIFT + REC = save | SHIFT + FILE |
| REC = the sampling pop-up | REC = live recording; sampling is the SAMPLER and the recorder's setup (REC held there) |
| SAVE (FILE) loads the file into the track's slot; SAVE steps SAMPLE / RAZOR | B10 LOAD into the next free slot; B3 / B4 / B5 = BROWSE / EDITOR / RAZOR |
| Upper T1 / T2 = memorize / recall | SHIFT + B3 / B4 |
| Upper S1 / S2 = copy / paste the pattern | F1 / F2 in PATTERN LAUNCH |
| Upper MST = tap tempo | SHIFT + B1 |
| SHIFT + PATN = name the pattern | F3 + PRESETS |
| Hold a track key = mute, double-tap = solo | MIX: hold = mute, SHIFT + the key = solo |
| **Ghost trigs** (SHIFT + a step) | gone: SHIFT + a white key picks a target; lock-only steps now come from live REC's automation |
| **Hold a step + the loop keys** = lock the step's region / slices | gone; live REC writes a loop key's position as locks |
| **Slice pads held** = the slice program (its locks, CHOK, SMUT), copy / clear / paste of it | gone |
| **The scopes** (steps / pads / PAGE / PATN held + REC copy, PLAY clear, SHIFT + PLAY paste) | F1 / F2 with the one-item buffer (steps, PAGES' pages, patterns) |
| **The pattern-clear question** (CLEAR P01?) | gone (no pattern clear on the panel) |
| The scene row on the lower keys (MST > SCENE): hold A / B + PLAY clears, SHIFT + A / B captures | CROSSFADER: SHIFT + A / B clears, B11 + A / B captures; the fader on W1-W12 with pressure |
| SCN A / SCN B on MOD KEYS | CROSSFADER's A / B held |
| RAZOR: SHIFT + a pad mutes the slice | gone (see the differences) |
| 16 patterns, 16 pool slots, a 96 MiB store | 64 patterns (4 banks), 128 slots, 219 MiB (0.6.1: 200) |

The 0.5.0 and older controls (and the 0.3.0 "What moved" table) are in git history: this file before 0.6.0, and
docs/UX-REDESIGN.md, docs/MOD-MATRIX.md.

## Differences from the norns version

- **The panel.** The grid's rows (loop keys, steps, keyboard, patterns, the crossfader) are modes on the 16 white
  keys. The norns K2 / K3 page navigation is four knobs in lanes of four (SEL steps them); the K1 + E2 / E3 snap is
  SHIFT + a knob, with the same lists. Undo (the grid's NO) is SHIFT + F1, and there is a redo (SHIFT + F2).
- **Razor.** The table is per track (the original shared one); split / delete / per-slice play mode and pitch are
  added.
- **FX.** The bus FX (S1, S2, MST) are global and not lockable; they morph with the scenes. The FX list is the
  original's 18 (DRIVE removed).
- **Patterns.** Page loops play (the original stores them but ignores them). Per-page rates are not done.
- **Scenes.** The scene base is the track value itself.
- **Sampling** is new (the original's REC only toggled an indicator), and so is live recording.
- **Not ported.** Names use A-Z 0-9 and a few signs (the font has no lower case). Undo covers params, globals, the
  fader, step edits and pastes; not scene edits, pattern operations, sample loads or recalls.
- The engine's own deviations are listed in src/dsp/README.md.

## Differences from docs/UX-V2.md

What the code does where the spec says otherwise (or says nothing):

- **FX's black keys** (0.6.3): B1-B4 the EFFECTS rows INSERT 1, INSERT 2, SEND 1, SEND 2 (the send rows select the
  bus), B5 steps SENDS and MASTER's knob pages (the spec's "toggles": MASTER P2 .. are on the same key, so its knob
  pages are reachable by B5 too).
- **SENDS's AMP block** (0.6.3): the track's level and pan act at the filter's output, before the inserts (as since
  0.2), so PRE taps after them too; the picture draws AMP after the inserts as the spec does.
- **A source key's tap** (0.6.1) opens its page only inside the mode that has it (MODULATION: LFO 1, LFO 2, VEL, AT
  = the MATRIX, M1-M4 = the macro's page; AMP: MOD ENV). Elsewhere a tap does nothing: no key changes the mode.
- **The header's held-key text** shows 0.15 s into a hold (a tap shows none); a message newer than the last key
  press wins over it, an older one gives way.
- **AMP and TRIG have no B11** (unused). LOOP ENGINE (0.6.41): B1 TRIG, B2 LOOP, B3 MACHINE, B4 WARP, B5 RANGE; their holds: ADVANCE, the loop mode, the machine, the warp mode, the voice mode.
- **LOOP ENGINE's zoom** (B11) changes the track's LOOP DIV param (16 ↔ 32), so it is saved and heard.
- **MIX's rates:** B1-B8 are eight rates (1/8 .. 4X), **B9 slower, B10 faster**, B11 the CAT (the spec had B1-B9 the
  list and B10 / B11 slower / faster, with B11 also the CAT).
- **MIX: a hold on S1, S2 or MST selects it** (buses have no mute).
- **EDIT's settings** are the project's MASTER settings (NAME, GLO BPM, BPM, RETURN MODE), not per-track settings.
- **ARP has no settings** (NO SETTINGS). The glide time is on W13-W16 and kept in the APP SETTINGS record.
- **SHIFT + FILE saves**; only a hold of FILE opens its settings, which are the recorder's setup (0.4's REC pop-up).
- **FILE while playing** opens BROWSE (0.6.11); tapping FILE again is the SAMPLER.
- **FILE's pages are on B3 / B4 / B5** (BROWSE / EDITOR / RAZOR). In the EDITOR every black key is a zoom or scroll
  key, so **SEL** goes back to BROWSE.
- **LOAD** fills the pool only (0.6.32: it set the selected track's slot before), and a file already in the pool is
  not loaded again.
- **Purge** refuses while playing (STOP FIRST), which matters in the SAMPLER.
- **The SAMPLER's length is on B1-B5** (FREE / 1 / 2 / 4 / 8 bars), not a settings row. It adds GAIN and MONITOR on
  the knobs.
- **RAZOR (FILE B5): the white keys are steps**, not the slice pads (the labels say SLICE n), and SHIFT + a key picks
  a target instead of muting the slice. So "Razor: as 0.4" holds for the knobs and F1 / F2, not the keys.
- **KEYBOARD** on a non-slice track plays the black keys as the sharps; the octave is -3..+3. Live REC writes KEYBOARD's notes only when the target is LIVE.
- **PATTERN LAUNCH:** SEL also picks the bank; stopped, a launch switches at once.
- **REC** toggles on its release (a hold opens the record settings).
- **The record settings** add CLICK LVL; METRONOME is OFF / REC / ALWAYS, its sound is in APP SETTINGS. They are
  saved with the project. COUNT-IN applies to FIXED and OVERDUB started from stop (CAPTURE has none).
- **APP SETTINGS** adds INTRO; KNOB SPEED is SLOW / MED / FAST; METRO SOUND is WOOD / BEEP / CLICK / COWBELL. Any
  other button closes it (keeping the values).
- **The arena** is 219 MiB (the spec said about 224), so about 21.7 minutes of stereo. (0.6.1: 200 MiB, about 19.8
  minutes, beside the project file's 11 MiB pack buffer and the second inserts' 8 MiB of engine.)
- **CROSSFADER's A / B held** shows the page of the mode you came from (FILTER's when that mode has no template).
