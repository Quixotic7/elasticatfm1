# Elasticat on the FM-1: controls (0.4.0)

The FM-1 is the panel; the sound runs in the RemoteFM1 host. Samples come from your `RemoteFM1/audio/` folder. Six
tracks **T1-T6**, each with its own sample, machine, warp mode, filter, amp, insert FX, sends, MOD and 16-step pages;
two send buses **S1**, **S2** and the master insert **MST**; 16 patterns; two scenes **A / B** with a crossfader. A
new project loads the first file in `/audio` onto T1.

0.3.0 works like ChoralRoot and FIZZ (docs/UX-REDESIGN.md has the why and the owner's decisions). The rules:

- **GLO is SHIFT.** Held with a knob: the value snaps to useful values. Held with a button or key: that key's
  shortcut. While it is held only the shortcut keys light, and after a quarter second alone its map shows.
- **A category button opens its page; tapped again: its next page.** Held for 0.4 s (or with SHIFT): its **setup**,
  which stays open on release.
- **Lanes.** A page shows four values at a time on KNOB 1-4, the active lane; **SELECT steps the lanes and runs on
  into the next page**, so SELECT alone reaches everything in a category. The tabs over the right-hand cell show the
  lanes (the active one bright).
- **HOME** goes back to the main view (SRC with the waveform) from anywhere in one press, closing whatever is open.
  Each category keeps its page and lane.
- **Pop-ups stay open on release**: F3, the PATN overlay, a setup, naming, a question, REC (sampling). In a pop-up **OCT- = back /
  cancel, OCT+ = OK / yes**, HOME closes it. Two small arrows top right mark a pop-up.
- **Knobs accelerate**: turned slowly a click is the finest step; spun, the steps grow up to about a sixteenth of
  the range a click. Lists (machines, modes, waves, options) are always one item a click.
- **A held step owns the knobs**: an orange frame round the screen, the count of held steps top right, every turned
  value becomes that step's lock (a filled orange chip; an outlined chip = locked on some step). **OCT+ held + a knob
  clears that lock** (nothing held: on every step of the track).
- **A value turned while its cell isn't on the screen** (the MIX page, the PATN overlay) takes over the screen for a
  second: the value big, a row of stripes, its name.
- The host's panel shows what every control does right now (the live labels: SHIFT's shortcuts while it is held,
  the lane's value names on the knobs, the lower keys' layer).

## Knobs

| Knob | Does | With SHIFT |
|---|---|---|
| MASTER | Master volume | |
| SELECT | The next lane, on into the next page (the FILE list: the file cursor; RAZOR: the selected slice; a setup: its lanes; naming: the place; F3: the tempo) | |
| PRESETS | **The track's sample**: the next / previous file of the folder, loaded into the track's slot and heard at once (the name big on the screen; a fast spin loads once it rests) | **The pattern** (queued to the next change, see PAT) |
| ALGORITHM | Track: T1..T6, then S1, S2, MST (F3: the swing) | |
| KNOB 1-4 | The active lane's four values | Snap (below) |

**Snap** (SHIFT + a value knob): each click goes to the next useful value. The norns script's own snap lists where it
has them (P/T -24 -12 -7 0 7 12 24; STRT / END 0 8 16 32 64 96 120 128; R-ST / R-EN multiples of 8; CUT / RES /
DRIV / MIX 0 32 64 96 127; VOL 0 32 64 100 127; LEN 4 8 16 32 48 64 96 128 256; RATE 1/2 1 2; BPM 60 80 90 100 110
120 128 136 140 160 180; the harmonizer's intervals; the warp, grain, chop, wavetable and FX lists ...), else
ChoralRoot's rules: 0..127 to 0 16 32 .. 112 127, 0..128 and the bipolar values to steps of 8, semitones to octaves,
values with decimals to whole numbers (0..1 to quarters), a small range one step a click. Lists don't snap. Trim
(FILE T-ST / T-EN) and RAZOR's STRT / END take the finest step instead.

## Buttons

| Button | Tap | Tap again | Held 0.4 s / SHIFT + |
|---|---|---|---|
| SEQ | TRIG: TRIG (NOTE LEN VEL; ERST LRST FRST), COND (chance, condition, ratchet), MTRIG or SLICE | next page | setup: swing |
| SEL | SRC: SOURCE / MACHINE / WARP / RANGE (the waveform) | next page | **held with a lower key: the loop keys** (below); held 0.4 s and let go with no key used: setup (machine, warp, loop division, poly). SHIFT + SEL: the setup at once |
| EDIT | FILT: FILTER (curve) / F.ENV / F.MIX | next page | setup: filter machine, filter env mode, mix mode |
| ENV | AMP: envelope, VOL, PAN, sends (two lanes) | - | setup: env mode, env range, portamento, slice hold |
| FX | FX: the insert machine's params (a picture per machine) / SENDS: S1, S2 levels, TAP. On S1 / S2 / MST: that bus's machine | next page | setup: **the FX machine** (insert, or the selected bus's), send tap |
| LFO | MOD: LFO 1 / LFO 2 / MOD ENV / VELOCITY / MACROS / MACRO 1-4 | next page | (no setup: a hold is a tap) |
| SAVE | FILE: SAMPLE (the browser; SELECT is its cursor) | **on SAMPLE: loads the file under the cursor** into the track's slot; on the track's own file it steps the lanes, then the pages: SAMPLE (2 lanes), RAZOR (2 lanes), round | |
| ARP | PAT: length, rate, global length, change mode | - | setup: quantize, global length |
| HOME | Back to the main view (SRC), every pop-up closed | | |
| PLAY | Play / pause (on its release; resume keeps the position). With a scope held: **CLEAR** | | **Held 0.4 s: F3.** SHIFT + PLAY: **stop + back to step 1** (twice: all voices off); with a scope held: **PASTE** |
| REC | **The REC pop-up (sampling, below).** With a scope held: **COPY** | In the pop-up: arm / record now / stop | SHIFT + REC: **save the project now** |
| OCT- | The lower keys: STEPS ↔ **LOOP KEYS** (lit) | | **OCT- + OCT+ together: F3.** Held: the PAGE scope (REC / PLAY). SHIFT + OCT-: **undo** |
| OCT+ | The lower keys: STEPS ↔ **KEYS** (lit); both lit = **SCENE** | | Held + a knob: clear that lock. SHIFT + OCT+: **redo** |

In a setup the same button (or SHIFT + it) goes back to the category's pages, OCT- / OCT+ close it, another button
opens that category's pages. Warp mode, filter machine and filter mix mode change only while stopped (the list says
STOP). An FX machine changes any time (a short fade, no click).

**On the RAZOR page** the OCT keys are its own (OCT+ split, OCT- delete; SHIFT: Auto-Chop / Redistribute; not undo
/ redo there) and a pop-up's OCT keys still come first.

<!-- layout:begin (generated by tools/layout/gen_layout.py from design/layout/elasticat.json: edit the design, not this) -->

## Pages and knobs (the layout "elasticat-0.4.0")

Each category button opens its first page; tapping it again goes to the next page. A page has one or more lanes of four values on KNOB 1-4; SELECT steps the lanes and runs on into the next page. Where a page depends on the track's machine, warp mode, filter machine, envelope mode or FX machine, each variant has its own rows. Setup: hold the category button, or SHIFT + it.

### TRIG

| Page | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| TRIG | 1 | NOTE | LEN | VEL | - |
|  | 2 | ERST | LRST | FRST |  |
| COND | 1 | CHNC | COND | RTCH |  |
| MTRIG (as SLICE) · SLICE/RAZOR | 1 | SLIC | PLAY | RTCH | SREV |
| MTRIG · otherwise | 1 | JUMP | RLSE |  |  |
| SETUP | 1 | SWING |  |  |  |

### SRC

| Page | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| SOURCE · NEIGHBOR/INPUT | - | | | | |
| SOURCE · SLICE/RAZOR | 1 | P/T | PLAY | SLIC | SLOT |
|  | 2 | CNT | CHOK | SMUT | SREV |
| SOURCE · otherwise | 1 | P/T | STRT | END | SLOT |
|  | 2 | XFAD | REV |  |  |
| MACHINE · NEIGHBOR/INPUT | - | | | | |
| MACHINE · SLICE/RAZOR | 1 | SLIC | PLAY | SREV | RTCH |
|  | 2 | SYNC | RATE |  |  |
| MACHINE · otherwise | 1 | LREV | XFAD | RLSE |  |
| WARP · NEIGHBOR/INPUT | - | | | | |
| WARP · TAPE | 1 | RATE | MFAD |  |  |
| WARP · VARI | 1 | RATE | MFAD |  |  |
| WARP · DOMINO | 1 | SLCS | SLEN | LOOP | RATE |
|  | 2 | ATK | REL | GATE |  |
| WARP · GRAIN | 1 | GDEN | GSIZ | SPRD | RATE |
|  | 2 | GSPD | RAND | DIR |  |
| WARP · OLA | 1 | CHAOS | OWIN | OWAN | RATE |
| WARP · PITCH | 1 | SMEAR | PCWN | PCDS | RATE |
| WARP · HARM | 1 | MIX | IN1 | IN2 | IN3 |
|  | 2 | RATE |  |  |  |
| WARP · WTBL | 1 | MORF | WSIZ | CYCW | LRAT |
|  | 2 | LDEP | LSHP | XFAD | RATE |
| WARP · FREEZE | 1 | FRZE | BLUR | RATE |  |
| WARP · FORMNT | 1 | FORM | FRZE | RATE |  |
| WARP · GSTR | 1 | STRCH | PWIN | SMTH | RATE |
| WARP · GSTR2 | 1 | STRCH | PWIN | SMTH | RATE |
| WARP · CHOP | 1 | SLEN | GATE | LOOP | RATE |
|  | 2 | ATK | REL |  |  |
| RANGE · NEIGHBOR/INPUT | - | | | | |
| RANGE · otherwise | 1 | R-ST | R-EN |  |  |
| SETUP | 1 | MACH | WARP | LDIV | POLY |

### FILT

| Page | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| FILTER · CLASSC | 1 | CUT | RES | TYPE | DPTH |
|  | 2 | DRIV |  |  |  |
| FILTER · MORPH | 1 | CUT | RES | MRPH | DPTH |
|  | 2 | DRIV |  |  |  |
| FILTER · LADDER | 1 | CUT | RES | DRIV | DPTH |
| FILTER · COMB | 1 | TUNE | FB | DAMP | DPTH |
|  | 2 | MIX |  |  |  |
| FILTER · FORMNT | 1 | CUT | RES | VOWL | DPTH |
|  | 2 | MIX |  |  |  |
| FILTER · SVF | 1 | CUT | RES | TYPE | DPTH |
|  | 2 | DRIV |  |  |  |
| F.ENV · F.ENV ADSR | 1 | ATK | DEC | SUS | REL |
|  | 2 | DPTH |  |  |  |
| F.ENV · otherwise | 1 | ATK | HOLD | REL | DPTH |
| F.MIX | 1 | BAL | MIX |  |  |
| SETUP | 1 | MACH | FENV | MODE |  |

### AMP

| Page | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| AMP · ENV ADSR | 1 | ATCK | DECAY | SUST | REL |
|  | 2 | VOL | PAN | SND1 | SND2 |
| AMP · otherwise | 1 | ATCK | HOLD | REL | VOL |
|  | 2 | PAN | SND1 | SND2 |  |
| SETUP | 1 | ENV | RANGE | PORT | SHLD |
|  | 2 | HOLD |  |  |  |

### FX

| Page | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| FX · DELAY | 1 | TIME | FBK | TONE | MIX |
| FX · REVERB | 1 | SIZE | DAMP | MIX |  |
| FX · LOFI | 1 | BITS | RATE | MIX |  |
| FX · COMP | 1 | THRS | RATO | ATK | REL |
|  | 2 | MKUP | MIX |  |  |
| FX · DESTRY | 1 | CRSH | SRR | WARB | DRIV |
|  | 2 | FOLD | SAT | TONE | LEVL |
| FX · ECHO | 1 | TIME | OFST | FBK | MODE |
|  | 2 | TONE | WOBL | MIX |  |
| FX · B.HOLE | 1 | SIZE | GRAV | MOD | LOW |
|  | 2 | HI | PRE | MIX |  |
| FX · CHORUS | 1 | RATE | DPTH | WDTH | TONE |
|  | 2 | MIX |  |  |  |
| FX · FLANGR | 1 | RATE | DPTH | FBK | TONE |
|  | 2 | MIX |  |  |  |
| FX · PHASER | 1 | RATE | DPTH | CNTR | STGS |
|  | 2 | FBK | MIX |  |  |
| FX · DJ EQ | 1 | LOW | MID | HIGH | XLOW |
|  | 2 | XHI |  |  |  |
| FX · DUCK | 1 | AMNT | SENS | ATK | REL |
| FX · T.ECHO | 1 | TIME | FBK | WOW | FLUT |
|  | 2 | SAT | TONE | MIX |  |
| FX · CASSET | 1 | WOW | FLUT | NOIS | CRKL |
|  | 2 | DROP | TONE | MIX |  |
| FX · MOTION | 1 | WDTH | RATE | TREM | PAN |
|  | 2 | SHPE |  |  |  |
| FX · RINGS | 1 | MODE | FREQ | FINE | FBK |
|  | 2 | TONE | MIX |  |  |
| FX · LIMIT | 1 | GAIN | CEIL | REL |  |
| FX · otherwise | - | | | | |
| SENDS | 1 | SND1 | SND2 | TAP |  |
| SETUP | 1 | INSERT FX | SEND TAP |  |  |

### MOD

| Page | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| LFO 1 | 1 | DEST | WAVE | SPD | DEP |
|  | 2 | MODE |  |  |  |
| LFO 2 | 1 | DEST | WAVE | SPD | DEP |
|  | 2 | MODE |  |  |  |
| MOD ENV | 1 | ATCK | DECAY | SUST | REL |
|  | 2 | DEST | DEPTH |  |  |
| VELOCITY | 1 | DEST | DEPTH |  |  |
| MACROS | 1 | M1 | M2 | M3 | M4 |
| MACRO 1 | 1 | VAL | PTCH | CUT | RES |
|  | 2 | AMP | PAN |  |  |
| MACRO 2 | 1 | VAL | PTCH | CUT | RES |
|  | 2 | AMP | PAN |  |  |
| MACRO 3 | 1 | VAL | PTCH | CUT | RES |
|  | 2 | AMP | PAN |  |  |
| MACRO 4 | 1 | VAL | PTCH | CUT | RES |
|  | 2 | AMP | PAN |  |  |

### MST

| Page | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| MIX | 1 | VOL | PAN | MUTE | BPM |
| MASTER | 1 | BPM | SWNG | GLEN | RTN |
| SCENE | 1 | XFD |  |  |  |
| SETUP | 1 | RTN | BPM | GBPM | NAME |

### FILE

| Page | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| SAMPLE | 1 | BPM | STEP | GAIN | SLOT |
|  | 2 | T-ST | T-EN |  |  |
| RAZOR | 1 | STRT | END | PLAY | ZOOM |
|  | 2 | SNAP | CHOK | P/T | SENS |

### PAT

| Page | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| PATTERN | 1 | LEN | RATE | GLEN | QUANT |
| SETUP | 1 | QUANT | GLEN |  |  |

### Buses (S1, S2, MST: every category but MST, FILE, PAT shows the bus's page)

| Page | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| S1 SETUP | 1 | SEND 1 FX |  |  |  |
| S2 SETUP | 1 | SEND 2 FX |  |  |  |
| MST SETUP | 1 | MASTER FX |  |  |  |
| S1: SEND 1 | | the lanes of FX/FX, for the bus's own FX machine | | | |
| S2: SEND 2 | | the lanes of FX/FX, for the bus's own FX machine | | | |
| MST: MASTER | | the lanes of FX/FX, for the bus's own FX machine | | | |
| MST: SRC = MIX | 1 | VOL | PAN | MUTE | BPM |

### F3

| Page | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| F3 | 1 | LEN | RATE | GLEN | QUANT |

<!-- layout:end -->

## F3 (OCT- + OCT+ together, or PLAY held)

The timing menu. It stays open; OCT- / OCT+ / HOME close it; PLAY still plays and stops.

| Control | Does |
|---|---|
| SELECT | The tempo (BPM; SHIFT snaps) |
| ALGORITHM | The swing |
| KNOB 1 / 2 | The track's pattern **LEN** (steps) / **RATE** (1/16 .. 2x) |
| KNOB 3 / 4 | The global length (GLEN) / the pattern change mode (QUANT) |
| Lower key tapped | Edit that step page (the keys and the screen follow) |
| Lower key held 0.4 s | That page into / out of the playing loop (never empty) |

The screen: the tempo and swing big, the pattern's pages (filled = in the loop, bright = the one you edit, orange
underline = playing), the four cells.

## SHIFT's keys (lit while SHIFT is held)

| SHIFT + | Does |
|---|---|
| OCT- / OCT+ | **Undo / redo** (16 deep, per gesture: a knob turn, a fader glide, a step tap, a lock, a paste, a page clear; a pause of a second starts a new gesture; a new edit drops the redo) |
| upper T1 / T2 | **Memorize** the whole project to RAM / **recall** it (also while playing: the pattern restarts at step 1; only changed samples reload). T2 lights once something is memorized |
| upper S1 / S2 | **Copy** the pattern / **paste** it onto the current one |
| upper MST | **Tap tempo** (the average of the last four taps; the tempo big on the screen) |
| FILL | **Latch** fill on / off |
| PATN | **Name the pattern** |
| PRESETS | The pattern |
| a value knob | Snap |
| PLAY / REC | Stop + step 1 (with a scope: paste) / save (with a scope: copy) |
| a category button | Its setup |

SHIFT with the lower keys keeps their own meanings: a step tapped = a **ghost trig**; in the loop layer, keys / pads
**17-32**; on the scene row, **store** A / B; on RAZOR, **mute** the slice; a glide key: the slow, linear glide.

The map (SHIFT held alone) draws the panel small: the upper keys with their icons (memorize, recall, copy, paste,
tap, latch, name), the buttons (undo, redo, stop, save, setup), PRESETS (patterns) and the knobs (snap).

## Upper keys

| Key | Tap | Hold 0.4 s | Double-tap |
|---|---|---|---|
| T1-T6 | Select the track (steps, pages and knobs follow; on release) | **Mute / unmute** | **Solo / unsolo** (soloed: the key blinks; when any track is soloed only soloed ones play) |
| S1, S2 | Select the send bus: its FX page (any category but MST / FILE / PAT) | | |
| MST | **The MST category**: MIX, then MASTER (BPM, swing, global length, playhead return), then SCENE | its setup: RTN, BPM, GBPM, NAME | |
| FILL | Held: fill (FILL / !FILL trig conditions). **FILL held + a track key: every mute and solo off** | | |
| PATN | The pattern overlay; **it stays open** until a pattern is picked, PATN again, OCT- / OCT+ or HOME. Held while you pick or copy: it closes on release. Held: the pattern scope (REC / PLAY / SHIFT + PLAY) | | |

The master bus is selected with ALGORITHM (after S2). Mutes are the pattern's (saved); solos are the session's.

## Lower keys (16)

**STEPS** (default): quick tap = trig on / off (a new trig keeps the key's velocity); **SHIFT + tap = ghost trig**
(plays nothing new, carries the last note's locks). Hold a step + turn a knob = p-lock. Holding a trig step while
stopped plays it (step preview).

**Hold SEL = loop keys 1-16** (the live scrub): hold one key to loop that slice of the region, two or more to span
them; let go and the playhead returns per MASTER > RTN (return / boomerang / reset). While stopped they audition.
On slice machines (SLICE, S.POLY, RAZOR, R.POLY) the keys are **slice pads**, velocity sensitive. **SHIFT + key =
17-32** when the track has them (loop division above 16, more than 16 slices).

**Hold a step, then hold SEL and press loop keys** = lock that step's region: first tap start + end, the same key again
start only (end follows the track), two keys = their span. On slice machines it picks the step's slice(s) (mono
machines: one, poly: several).

**LOOP KEYS** (OCT- lit): the loop keys without holding SEL.

**KEYS** (OCT+ lit): a white-key keyboard, -12..+14 semitones, centre C on key 8. Slices play the track's slice at
that pitch; loops transpose while held; WAVETABLE plays its oscillator. Hold a step + a key = lock the step's pitch.

**SCENE** (both OCT LEDs lit): the crossfader row, see Scenes. On MST > SCENE the lower keys are the scene row whatever
the key mode; on RAZOR they are the razor pads; in F3 the pages; with the PATN overlay the patterns.

## Sources: machines and warp modes

Machines (SRC setup, KN1): **LOOP**, **TRIG**, **SLICE** / **S.POLY** (even slices), **RAZOR** / **R.POLY** (the
razor table's slices, mono / up to 8 voices), **NEIGH** (T2-T6), **INPUT** (the live input; on T1 the list skips
NEIGH). Warp modes (SRC setup, KN2, while stopped): TAPE,
TEMPO VARISPEED, DOMINO, GRANULAR, RANDOM OLA, PITCH CORRECT, HARMONIZER, WAVETABLE, SPECTRAL FREEZE, FORMANT,
GSTRETCH, GSTRETCH 2, CHOPPED.

The WARP page (SRC page 3) shows each mode's params in the original's layout, in lanes of four: DOMINO SLCS SLEN LOOP
RATE / ATK REL GATE; RANDOM OLA CHAOS OWIN OWAN RATE; PITCH CORRECT SMEAR PCWN PCDS RATE; HARMONIZER MIX IN1 IN2 IN3 /
RATE (an interval of 0 = that voice off); WAVETABLE MORF WSIZ CYCW LRAT / LDEP LSHP XFAD RATE (pitch from P/T, base
C3); SPECTRAL FREEZE FRZE BLUR RATE; FORMANT FORM FRZE RATE; GSTRETCH / GSTRETCH 2 STRCH PWIN SMTH RATE. CYCW and LRAT
move in semitone ratios. The spectral modes are limited to a few tracks at a time, as the original limits them.

**NEIGH**: the track's source is the track to its left, whole: after its filter, amp, insert FX, mute and pan (the
original re-points the left track's mix output). The left track is then heard only through this one: this track's
filter, insert, amp and pan shape it, its mute silences both; the left track's own sends still feed S1 / S2. SRC shows
the routing and both levels. Chains work (T2 into T3 into T4). A NEIGH track never goes idle.

**INPUT** (0.4.0): the track's source is the host's live audio input (the phone's microphone, an audio interface, on
iOS another plugin's audio), stereo, as the original's INPUT machine: it plays whether the transport runs or not, with
no envelope, warp or pitch; the track's filter, insert FX, amp, pan and sends shape it. Frame for frame: what comes in
goes out in the same audio block. The host does not monitor the input itself: an INPUT track **is** the monitor (as
the original turned norns' own monitoring off while a track used INPUT). SRC shows the input's L / R meters, the
track's output and the host's latency; **NO INPUT** in orange when the host has none (off, not allowed, unplugged):
the track is then silent. More than one track may use INPUT (each its own filter / FX). A host without input (or a
cart opened before the host had it): the machine is there and silent.

## REC (sampling)

REC with no scope held opens the **REC pop-up** (it stays open; HOME, OCT- or a category button closes it; a take
goes on when it closes, the REC button lit). It records the live input, or the master out (resample), straight into a
pool slot:

| Control | In the REC pop-up |
|---|---|
| REC / OCT+ | **ARM**; armed: **REC NOW** (start at once, whatever SYNC says); recording: **STOP** (keep the take) |
| OCT- | idle: **CLOSE**; armed: **DISARM**; recording: **DISCARD** the take |
| SELECT | the target slot, POOL 01-16 (default: the track's own slot; it follows the track until SELECT moves it) |
| KN1 SRC | **IN** (the live input) / **MST** (the master out, after the master insert and volume: resample) |
| KN2 LEN | **FREE** (until stopped) / **1, 2, 4, 8, 16 BAR** (a bar = 4 beats at the tempo when the take starts) |
| KN3 THR | OFF / **-60 .. -6 dB** (6 dB steps): with SYNC FREE an armed take starts on the first frame over it |
| KN4 SYNC | **PLAY** (the take starts with PLAY, on step 1) / **BAR** (the next bar line while playing) / **FREE** (at once, or at the threshold) |

SHIFT works as everywhere (SHIFT + REC still saves the project). PLAY and the keys stay live in the pop-up, so you can
play along, arm, then press PLAY.

- **The screen** (the mockup's REC screen): IN L+R (or MASTER) > POOL nn, the take's clock in orange (REC 0:07), the
  waveform drawn as it comes in with the orange record head, the bar ruler, the input meters with the threshold's
  tick, the latency; armed: what it waits for (WAIT PLAY / BAR / -24DB); idle: what the slot holds now (dim) and MAX,
  the longest take that fits.
- **Latency:** a synced take of the input is shifted by the latency the host reports (the round trip: what the
  cart played comes back that many frames later), so what was
  played on step 1 lands on the take's frame 0 (a resample needs no shift).
- **When it ends** (the length reached, STOP, or the memory full) the take goes into the slot at once: the slot's
  tracks play it (its BPM is the tempo, its steps the length), the waveform shows. Then it is saved as a WAV in your
  RemoteFM1 folder, `data/elasticat/rec/rec-NNNN.wav` (`/data/rec/` to the cart, 16-bit stereo 44.1 kHz, the next
  free number), written in pieces in the background, never holding up the sound; the project keeps its path, so it
  reloads like any sample (**SAVED REC-0001.WAV**; **NOT SAVED: ...** when the host has no folder: the take still
  plays until the project is reloaded).
- **Memory:** takes go into the sample store (96 MiB, about 9.5 minutes of stereo with an empty pool). An armed FREE
  take reserves all that is free; a take with a length reserves its bars + a quarter. While a take is armed or being
  saved, loading a sample that does not fit fails instead of repacking the store.
- The settings (SRC, LEN, THR, SYNC) are not saved with the project.

## FILE / RAZOR: the samples and the razor slice editor

**PRESETS** steps the track's sample through the folder from any page. SAVE opens FILE, **SAMPLE**: SELECT moves the
cursor, SAVE loads the file under it into the track's slot (BPM STEP GAIN SLOT, then T-ST T-EN on lane 2). When the
cursor is on the track's own file, SAVE steps the lanes and then opens **RAZOR** for that file; SAVE there steps its
two lanes and goes back to SAMPLE. The razor machines (RAZOR, R.POLY) play each slice from its own start / end. The
table is per track and saved with the project; its per-slice settings (PLAY, CHOK, P/T, mute) apply to every slice
machine.

| On RAZOR | Does |
|---|---|
| Lower keys 1-16 | Select that slice and play it (velocity); slices 17-32 when the selected slice is in that bank |
| SHIFT + lower key | Mute / unmute the slice in the razor table (it stays on its steps, plays nothing) |
| SELECT | Select the slice |
| Lane 1: KN1 / KN2 | **STRT / END** of the selected slice (0..1 of the track window), under the SNAP mode |
| Lane 1: KN3 | **PLAY**: the slice's play mode (TRACK = the track's) |
| Lane 1: KN4 | **ZOOM** 1X..64X, centred on the point you last moved |
| Lane 2: KN1 | **SNAP**: GRID (next slice_count division), ZOOM (0.5 / zoom per detent, the plain mode), ZERO-X (the nearest zero crossing), TRANS (the next / previous transient), FRIEND (also drags the touching neighbour point) |
| Lane 2: KN2 | **CHOK**: the slice's choke group A-H (a new voice in the group cuts the others; OFF) |
| Lane 2: KN3 | **P/T**: the slice's pitch offset |
| Lane 2: KN4 | **SENS**: Auto-Chop / transient sensitivity (the ticks over the waveform) |
| OCT+ | **Split** the selected slice at the point you last moved (else in the middle) |
| OCT- | **Delete** the selected slice |
| SHIFT + OCT+ | **Auto-Chop**: one slice per transient (the head before the first one left out), the slice count follows |
| SHIFT + OCT- | **Redistribute**: the slices in use as even divisions |

Split and delete renumber the slices after it; the track's steps follow (a step on slice 6 stays on that audio).
The screen: the waveform of the track window (trim x Range), the selected slice as a cream block, slice numbers,
the transients at SENS as ticks, the sounding voices in orange, the zoom window as a bar, the 16 pads of the bank.

## FX and sends

- **18 FX machines** (NONE, DELAY, REVERB, LOFI, COMP, DESTROY, ECHO, BLACKHOLE, CHORUS, FLANGER, PHASER, DJ EQ, DUCK,
  TAPE ECHO, CASSETTE, MOTION, RINGS, LIMIT) on four kinds of slot: each track's insert, the S1 and S2 send returns,
  the master insert. The machine is chosen in the setup (FX held, or SHIFT + FX; KN1) so a knob turn never swaps it
  by accident. On a send, NONE returns nothing.
- The FX page shows the machine's cells (lanes of four) over a picture: the delay taps, the reverb's decay, the
  compressor / limiter curve with the live IN and GR meters, the crushed wave, the sweep, the EQ bands; the bar at
  the right is the slot's output level.
- **SENDS** (FX, page 2): S1 / S2 levels and TAP (PRE = after the filter, POST = after the insert). A muted track
  still feeds its sends (as the original). The MIX page meters the S1 / S2 returns.
- **DUCK** listens to the dry program: on a send return, the six tracks; on an insert, the tracks before it; on the
  master, the master.
- A stopped track that has gone quiet for 4 s stops using CPU until it plays again (the original's idle saver).

## MOD

Two LFOs (DEST, WAVE, SPD synced, DEP bipolar; MODE free / trig / one / hold on lane 2), the mod envelope (ADSR; DEST,
DEPTH on lane 2; retriggered where the step's ERST is on, the LFOs where LRST is on), velocity (slice trigs) and four
macros (MACRO n: VAL and the depths to PTCH CUT RES, then AMP PAN). DEST: PITCH (+-12 st), CUTOFF, RES, AMP, PAN, or
a macro (M1-M4). PITCH reaches every warp mode as in the original. The bar at the right of the LFO / env / velocity
pages is the live value at the destination. Depths, speeds, the env times and macro values p-lock; DEST, WAVE and
MODE do not (as the original).

## Scenes and the crossfader

Two scenes, A and B, each a sparse set of param values for every track and the three buses; the fader morphs between
them. **MST > SCENE** (the MST key, page 3) is the overview: the fader with its six glide positions and where a glide
is heading, and the params that morph; **KN1 = XFD**, the fader as a direct 0..128 knob (SHIFT: 0 32 64 96 128). The
scene row is on the lower keys in the SCENE key mode (a strip with the fader sits over the page) and always on MST >
SCENE: lower keys 9-16 = A, six glide keys, B (the grid's row 5), mirrored on 1-8.

| Gesture (scene row) | Does |
|---|---|
| SHIFT + A (key 1 / 9), SHIFT + B (key 8 / 16) | Capture every morphable param of every track and bus into the scene (sparse) |
| Hold A or B + turn a cell | That param (selected track) into the held scene only; the sound does not move until the fader does. The cell shows the scene's value as a filled chip while the anchor is held |
| Hold A or B + PLAY | Clear that scene |
| Hold a glide key (2-7 / 10-15 = 0, 0.2 .. 1) | Ease the fader there (5 /s exponential, lands in ~1 s); release freezes; another key still held takes over |
| SHIFT + hold a glide key | The slow, linear move: the whole A..B travel in 5 s |

Morphing is the base-value resolver's source 2: a firing step's p-lock wins, then the fader, then the track value.
The track value is never written; turning a param's knob by hand gives it back to the knob until the fader next
moves. Only continuous params morph. The scenes and the fader are saved with the project (one pair for all patterns)
and re-applied after a pattern switch.

## Copy / clear / paste (each scope keeps its own buffer)

| Scope held | REC = copy | PLAY = clear | SHIFT + PLAY = paste |
|---|---|---|---|
| Step(s) | the held steps with their locks | their p-locks (the trig stays) | onto the held step(s), the copied steps' spacing kept (past the page edge: dropped) |
| Slice pad(s) (loop layer, slice machines) | the pad's program: locks, choke, mute | the programs | onto the held pads |
| PAGE (an OCT key held) | the step page | the step page | onto the step page |
| PATN (held) | the pattern: params, steps, BPM, page loops, slice programs | **the trigs** of every track: a question, **CLEAR P01?**, that stays open: OCT+ yes, OCT- / HOME no | onto the current pattern (its name stays) |

SHIFT + S1 / S2 copy / paste the pattern without holding PATN. A toast in the middle of the screen says what happened.

## Naming

SHIFT + PATN names the pattern; MST setup NAME (turn) names the project. KN1 / SELECT the place, KN2 the character,
KN3 a random name, KN4 left = delete, right = insert a space; **OCT+ keeps it**, OCT- or HOME cancels.

## Slices and patterns

- Hold slice pad(s) (loop layer) + turn a lockable cell = that slice's own lock in the pattern. The slice SOURCE page
  has **CHOK** (choke group NONE, 1-8) and **SMUT** (mute) on its lane 2 for the held pads, else for the SLIC slice.
- A slice has two homes: the **razor table** (per track, every pattern: range, PLAY, CHOK, P/T, mute) and the
  **pattern's slice program** (locks, CHOK, SMUT). A slice is silent when either mutes it; the pattern's choke group
  wins, else the razor table's.
- BPM is **per pattern**: a pattern switch brings its BPM. MST setup **GBPM ON** = one BPM for all patterns.

## LEDs

Steps: trig = mid, p-locked trig = flicker, ghost = dim, playhead = bright. Loop keys: the region dim, the playhead's
key mid, held keys bright. F3: the page you edit lit, the loop's pages mid, the playing one blinks. RAZOR: the bank's
slices dim, the selected one bright, the sounding ones mid, muted ones faint. Upper keys: the selected track bright,
the others by their level, muted (or silenced by a solo) off, soloed blinking; MST lit on its category. Category
buttons: the current one lit (blinking in its setup). OCT- / OCT+: the key mode (OCT- loop keys, OCT+ keys, both
scene); in a pop-up OCT- lit and OCT+ blinking. PLAY green while playing. REC dim when the project has unsaved
changes, mid when a scope is held or the REC pop-up is open, **blinking while a take is armed, lit while recording**. **SHIFT held**: only its keys light (T1, T2 once memorized, S1, S2 once copied,
MST blinking, FILL, PATN; OCT- / OCT+ when there is something to undo / redo; PLAY, REC; the categories with a setup).

## Differences from the norns version

- **The panel.** The grid's rows (loop keys, steps, keyboard, pattern slots, the crossfader) share the 16 lower keys:
  SEL held, the OCT key modes, the PATN overlay and F3 switch them. STOP is SHIFT + PLAY; the grid's PAGE key is F3
  (and an OCT key held for the page scope); the norns K2/K3 pair navigation is four knobs on lanes of four (SELECT
  steps them); the norns K1 + E2 / E3 snap is SHIFT + a knob, with the same lists. Undo (the grid's NO) is SHIFT +
  OCT-, and there is a redo (SHIFT + OCT+); memorize / recall are SHIFT + T1 / T2. Solo is added (a double-tap).
- **Razor.** The table is per track (the original shared one global table); the plain STRT / END turn is the ZOOM
  snap and the other snaps act on every turn; split / delete / per-slice play mode + pitch are added; the onset
  envelope reads 32 frames per window (the Lua 8).
- **FX.** The bus FX (S1, S2, MST) are global and not p-lockable (the original's bus lanes are not ported); they do
  morph with the scenes, as in the original. The macro depth matrix is edited on the MACRO 1-4 pages (the original
  used a grid gesture). The FX machine list is the original's 18 (DRIVE removed).
- **Patterns.** The pattern scope of copy / clear / paste is PATN held (or SHIFT + S1 / S2); the STOP-held CUT/PASTE
  mode is not ported. Page loop plays (the original stores it but ignores it in playback); F3's KN2 sets the track's
  pattern rate. Per-page rates are not done.
- **Samples.** PRESETS steps the track's sample through the folder (the original picks a file in its browser only).
- **Scenes.** The scene base is the track value itself (the original's stashed base always equals it). Hold an anchor
  + PLAY clears the whole scene (the original clears one param of both scenes with K2/K3).
- **Slices.** Slice mute is the SMUT cell or SHIFT + key on RAZOR. Slice locks apply to live pads for pitch /
  velocity only.
- **Sampling** (0.4.0) is new: the original's REC only toggled an indicator ("recording itself is a future
  feature"); the REC pop-up follows the design's REC screen (design/mockup/Display.dc.html "rec", Sample.dc.html).
- **Not ported.** Live step trig (an editor pref), the step preview pref, the visualizer. Names use
  A-Z 0-9 and a few signs (the 3x5 font has no lower case). Undo covers params, globals, the fader, step edits,
  pastes and page clears; not scene edits, pattern-scope operations, sample loads or recalls (as the original).
- The engine's own deviations (control rate, feedback loops, click-free swaps, ...) are listed in src/dsp/README.md.

## What moved in 0.3.0 (from 0.2.0)

| 0.2.0 | 0.3.0 |
|---|---|
| FN (GLO) + KN1-4 = cells 5-8 | lanes: SELECT steps them; SHIFT + a knob snaps |
| SELECT = the page | SELECT = the lane, on into the next page |
| FN + a category = setup | the same, or hold the button 0.4 s (SRC: held and let go with no loop key used) |
| HOME = the MST category | HOME = back to SRC; the MST upper key = the MST category (hold: its setup) |
| MST upper key = the master bus | ALGORITHM past S2 |
| PRESETS = the pattern | PRESETS = the track's sample; SHIFT + PRESETS = the pattern |
| OCT taps cycle STEPS / LOOP / KEYS / SCENE | OCT- toggles LOOP KEYS, OCT+ toggles KEYS, both = SCENE |
| OCT held = the PAGE overlay (SELECT: SEL / LOOP / RATE) | F3 (OCT- + OCT+, or PLAY held): tap a key = edit the page, hold = loop it, KN2 = rate; an OCT key held stays the PAGE scope |
| PLAY acts on its press | on its release (a hold opens F3) |
| FN + OCT+ / OCT- memorize / recall | SHIFT + T1 / T2 |
| FN + SAVE undo | SHIFT + OCT- (SHIFT + OCT+ redo) |
| FN + a track key = mute | hold a track key; double-tap = solo; FILL + a track key = all on |
| PATN overlay + SAVE = name the pattern; naming SAVE keeps, FN tap cancels | SHIFT + PATN; OCT+ keeps, OCT- / HOME cancel |
| CLEAR P01? PLAY again within 3 s | the question stays: OCT+ yes, OCT- / HOME no |
| FILE: SAVE on the track's file = RAZOR, RAZOR: SAVE = back | SAVE steps the lanes, then the pages (SAMPLE 2 lanes, RAZOR 2 lanes) |
| RAZOR FN + KN1-4 = SNAP CHOK P/T SENS | RAZOR lane 2 (SAVE) |
| locks in orange text, "LCK" | a filled chip (outlined: locked on some step), the orange frame, the held count |
