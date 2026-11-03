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

## Pages and knobs (the layout "elasticat-0.5.0")

Each category button opens its first page; tapping it again goes to the next page. A page has one or more lanes of four values on KNOB 1-4; SELECT steps the lanes and runs on into the next page. Where a page depends on the track's machine, warp mode, filter machine, envelope mode or FX machine, each variant has its own rows. Setup: hold the category button, or SHIFT + it.

### TRIG

| Page | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| TRIG | 1 | NOTE | LEN | VEL | - |
|  | 2 | - | ENV RESET | LFO RESET | FILT RESET |
| COND | 1 | CHANCE | COND | RATCHET |  |
| PLAYHEAD (as SLICE) · SLICE/RAZOR | 1 | SLIC | PLAY | RTCH | SREV |
| PLAYHEAD · otherwise | 1 | SNAP START | RET MODE |  |  |
| SETUP | 1 | BPM | SWING |  |  |

### SRC

| Page | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| LOOP · NEIGHBOR/INPUT | - | | | | |
| LOOP · SLICE/RAZOR | 1 | P/T | PLAY | SLIC | SLOT |
|  | 2 | CNT | CHOK | SMUT | SREV |
| LOOP · otherwise | 1 | PITCH | SLOT | START | END |
|  | 2 | XFADE | REVERSE |  |  |
| MACHINE · NEIGHBOR/INPUT | - | | | | |
| MACHINE · SLICE/RAZOR | 1 | CNT | PLAY | SREV | RTCH |
|  | 2 | SYNC | RATE |  |  |
| MACHINE · otherwise | 1 | LREVERSE | LXFADE | RET MODE |  |
| WARP · NEIGHBOR/INPUT | - | | | | |
| WARP · TAPE | 1 | WARP RATE | WARP FADE |  |  |
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
| WARP · CHOP | 1 | SLEN | GATE | RATE | - |
|  | 2 | ATK | REL |  |  |
| RANGE · NEIGHBOR/INPUT | - | | | | |
| RANGE · otherwise | 1 | - | - | R-START | R-END |
| SETUP | 1 | MACHINE | WARP MODE | LOOP DIV | VOICE MODE |

### FILT

| Page | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| FILTER · CLASSC | 1 | CUTOFF | RESO | TYPE | DRIVE |
| FILTER · MORPH | 1 | CUT | RES | MRPH | DPTH |
|  | 2 | DRIV |  |  |  |
| FILTER · LADDER | 1 | CUT | RES | DRIV | DPTH |
| FILTER · COMB | 1 | TUNE | FB | DAMP | DPTH |
|  | 2 | MIX |  |  |  |
| FILTER · FORMNT | 1 | CUT | RES | VOWL | DPTH |
|  | 2 | MIX |  |  |  |
| FILTER · SVF | 1 | CUT | RES | TYPE | DPTH |
|  | 2 | DRIV |  |  |  |
| ENV · F.ENV ADSR | 1 | ATK | DEC | SUS | REL |
|  | 2 | DPTH |  |  |  |
| ENV · otherwise | 1 | ATTACK | HOLD | RELEASE | DEPTH |
| MIX | 1 | STEREO BAL | WET |  |  |
| SETUP | 1 | FILT MODE | ENV MODE | MIX MODE |  |

### AMP

| Page | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| ENV · ENV ADSR | 1 | ATCK | DECAY | SUST | REL |
|  | 2 | VOL | PAN | SND1 | SND2 |
| ENV · otherwise | 1 | ATTACK | HOLD | - | RELEASE |
|  | 2 | PAN | SND1 | SND2 | VOL |
| SETUP | 1 | ENV MODE | ENV RANGE | PORTAMENTO | SLICE HOLD |
|  | 2 | HOLD TIME |  |  |  |

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
| SENDS | 1 | - | - | SEND 1 | SEND 2 |
| SETUP | 1 | INSERT 1 | - | SEND MODE |  |

### MOD

| Page | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| LFO 1 | 1 | WAVE | SPEED | DEPTH | MODE |
| LFO 2 | 1 | WAVE | SPEED | DEPTH | MODE |
| ENV | 1 | ATTACK | DECAY | SUST | RELEASE |
|  | 2 | - | - | - | DEPTH |
| VELOCITY | 1 | - | - | - | DEPTH |
| MACROS | 1 | M1 | M2 | M3 | M4 |
| MACRO 1 | 1 | VAL | PITCH | CUT | RES |
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
| TRACK MIX | 1 | VOL | PAN | MUTE | BPM |
| MASTER | 1 | BPM | SWNG | GLO LENGTH |  |
| SCENE | 1 | CROSSFADE |  |  |  |
| SETUP | 1 | NAME | GLO BPM | BPM | RETURNMODE |

### FILE

| Page | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| SAMPLE | 1 | BPM | STEP | GAIN | SLOT |
|  | 2 | - | - | TRIM START | TRIM END |
| RAZOR | 1 | PLAY | ZOOM | SLCE START | SLCE END |
|  | 2 | SNAP MODE | CHOKE | PITCH | TRANS SENS |

### PAT

| Page | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| PATTERN | 1 | LENGTH | RATE | GLO LENGTH | LNCH MODE |
| SETUP | 1 | LNCH MODE | GLO LENGTH |  |  |

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
| F3 | 1 | LENGTH | RATE | GLO LENGTH | LNCH MODE |

<!-- layout:end -->

<!-- params:begin (generated by tools/layout/gen_layout.py from design/layout/descriptions.json: edit that, not this) -->

## Parameter reference

What every value does, grouped by the category whose pages show it first (the designer shows the same text as tooltips). Label: the screen's own short name (a page may give it another).

### TRIG: The SEQ button: the step's own settings (note, length, velocity, resets), trig conditions and ratchets, and the trig-jump / slice page. Setup: swing.

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
| `default_length` | LEN | Note length | How long a step's note lasts, 1/4 to 16 steps: the envelope gate, and how long its region and p-locks hold. P-lockable. |
| `default_velocity` | VEL | Velocity | Trig velocity, 0 to 100%. Sets a slice's level and feeds the VELOCITY mod source (slice trigs only). P-lockable. |
| `global.bpm` | BPM | Tempo | The project tempo, 20 to 300 BPM (SHIFT snaps to common tempos). Saved per pattern unless GBPM is on. |
| `global.swing` | SWNG | Swing | Global swing, 50% (straight) to 75% (heavy): odd steps get longer, even ones shorter. |

### SRC: The SEL button: the track's sound source - pitch, loop region, sample slot, machine and warp-mode controls over the waveform. Setup: machine, warp mode, loop division, polyphony.

| Item | Label | Name | What it does |
|---|---|---|---|
| `machine` | MACH | Machine | What the track plays: LOOP, TRIG, SLICE, RAZOR, S.POLY, R.POLY, NEIGH (the left track) or INPUT (the live input). A setting, not p-lockable. |
| `sample_slot` | SLOT | Sample slot | Which of the 16 pool slots the track plays. Tracks sharing a slot share its sample and its file settings. |
| `loop_start` | STRT | Loop start | Where the loop region starts, 0 to 128 across the Range. P-lockable (a step can play its own region). |
| `loop_end` | END | Loop end | Where the loop region ends, 0 to 128 across the Range. P-lockable (a step can play its own region). |
| `range_start` | R-ST | Range start | The start of the Range, the window inside the sample's trim that loop points, loop keys and slices map onto (0 to 128). P-lockable. |
| `range_end` | R-EN | Range end | The end of the Range, the window inside the sample's trim that loop points, loop keys and slices map onto (0 to 128). P-lockable. |
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

### FILT: The EDIT button: the track's filter, its envelope and its stereo / mid-side spread. Setup: filter machine, filter envelope mode, mix mode.

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

### AMP: The ENV button: the amp envelope, volume, pan and the two send levels. Setup: envelope mode and range, portamento, slice hold.

| Item | Label | Name | What it does |
|---|---|---|---|
| `slice_hold_to_step` | SHLD | Slice hold to step | ON: a slice note lasts the step's note length (LEN). OFF: it lasts the HOLD time instead. |
| `slice_hold` | HOLD | Slice hold time | How long a slice note lasts when SHLD is off, 0 to 4 seconds. |
| `env_mode` | ENV | Amp envelope mode | ADSR (sustains while the note lasts) or AHR (attack, hold, release; ignores the note length). A setting, not p-lockable. |
| `env_attack` | ATCK | Amp attack | The amp envelope's fade-in, 0 up to the envelope range. P-lockable. |
| `env_decay` | DECAY | Amp decay | ADSR: the time to fall to the sustain level, 0 up to the envelope range. P-lockable. |
| `env_sustain` | SUST | Amp sustain | ADSR: the level held while the note lasts, 0 to full. P-lockable. |
| `env_release` | REL | Amp release | The fade-out after the note ends (ADSR) or after the hold (AHR); the top is INF (no fade). P-lockable. |
| `env_hold` | HOLD | Amp hold | AHR: how long the sound stays at full before releasing; the top is INF (holds until the next trig). P-lockable. |
| `env_range` | RANGE | Envelope range | The longest time the envelope knobs reach, 1 s to 1024 s (default 8 s). Applies to the amp, filter and mod envelopes. |
| `send1_level` | SND1 | Send 1 level | How much of the track goes to the S1 bus, off to full. P-lockable: throw single steps into the effect. |
| `send2_level` | SND2 | Send 2 level | How much of the track goes to the S2 bus, off to full. P-lockable. |
| `amp` | VOL | Volume | The track's level, 0 to 127 (default 100), times the slot's gain. P-lockable. |
| `pan` | PAN | Pan | Stereo position, L64 to R64; C = centre. P-lockable. |
| `portamento` | PORT | Portamento (legato) | ON: a trig that arrives while the amp envelope is still open extends the note instead of restarting the attack. |

### FX: The FX button: the track's insert effect (or, with S1 / S2 / MST selected, that bus's effect) and the send levels. Setup: the FX machine and the send tap.

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

### MOD: The LFO button: two LFOs, the mod envelope, velocity modulation and the four macros with their depth matrices. No setup.

| Item | Label | Name | What it does |
|---|---|---|---|
| `lfo1_wave` | WAVE | LFO 1 wave | SINE, TRI, SAW, RSAW (ramp down), SQR or RAND (sample and hold). Not p-lockable. |
| `lfo1_speed` | SPD | LFO 1 speed | One cycle per 8 bars down to 1/64, synced to the tempo. P-lockable. |
| `lfo1_depth` | DEP | LFO 1 depth | How much LFO 1 moves its destination, -64 to +64; 0 = off, negative inverts. P-lockable. |
| `lfo1_mode` | MODE | LFO 1 mode | FREE runs freely; TRIG restarts on steps with LRST on; ONE plays one cycle per trig then stops; HOLD latches its value at each trig. |
| `lfo2_wave` | WAVE | LFO 2 wave | SINE, TRI, SAW, RSAW (ramp down), SQR or RAND (sample and hold). Not p-lockable. |
| `lfo2_speed` | SPD | LFO 2 speed | One cycle per 8 bars down to 1/64, synced to the tempo. P-lockable. |
| `lfo2_depth` | DEP | LFO 2 depth | How much LFO 2 moves its destination, -64 to +64; 0 = off, negative inverts. P-lockable. |
| `lfo2_mode` | MODE | LFO 2 mode | FREE runs freely; TRIG restarts on steps with LRST on; ONE plays one cycle per trig then stops; HOLD latches its value at each trig. |
| `menv_attack` | ATCK | Mod env attack | The mod envelope's attack, 0 up to the envelope range. P-lockable. |
| `menv_decay` | DECAY | Mod env decay | The time to fall to the sustain level, 0 up to the envelope range. P-lockable. |
| `menv_sustain` | SUST | Mod env sustain | The level held while the note lasts, 0 to full. P-lockable. |
| `menv_release` | REL | Mod env release | The time to fall back after the note ends, 0 up to the envelope range. P-lockable. |
| `menv_depth` | DEPTH | Mod env depth | How much the mod envelope moves its destination, -64 to +64; 0 = off. P-lockable. |
| `mvel_depth` | DEPTH | Velocity depth | How much velocity moves its destination, -64 to +64; 0 = off. P-lockable. |
| `macro1_value` | M1 | Macro 1 | Macro 1's value, 0 to 127; moves pitch, cutoff, res, amp and pan by its MACRO 1 depths. LFOs and envelopes can also drive it. P-lockable. |
| `macro2_value` | M2 | Macro 2 | Macro 2's value, 0 to 127; moves pitch, cutoff, res, amp and pan by its MACRO 2 depths. LFOs and envelopes can also drive it. P-lockable. |
| `macro3_value` | M3 | Macro 3 | Macro 3's value, 0 to 127; moves pitch, cutoff, res, amp and pan by its MACRO 3 depths. LFOs and envelopes can also drive it. P-lockable. |
| `macro4_value` | M4 | Macro 4 | Macro 4's value, 0 to 127; moves pitch, cutoff, res, amp and pan by its MACRO 4 depths. LFOs and envelopes can also drive it. P-lockable. |
| `macro1_pitch_depth` | PTCH | Macro 1 to pitch | How far macro 1 moves pitch (up to +-12 semitones), -64 to +64; 0 = not at all, negative inverts. Not p-lockable. |
| `macro1_cutoff_depth` | CUT | Macro 1 to cutoff | How far macro 1 moves the filter cutoff, -64 to +64; 0 = not at all, negative inverts. Not p-lockable. |
| `macro1_res_depth` | RES | Macro 1 to resonance | How far macro 1 moves the filter resonance, -64 to +64; 0 = not at all, negative inverts. Not p-lockable. |
| `macro1_amp_depth` | AMP | Macro 1 to amp | How far macro 1 moves the track's level, -64 to +64; 0 = not at all, negative inverts. Not p-lockable. |
| `macro1_pan_depth` | PAN | Macro 1 to pan | How far macro 1 moves the pan position, -64 to +64; 0 = not at all, negative inverts. Not p-lockable. |
| `macro2_pitch_depth` | PTCH | Macro 2 to pitch | How far macro 2 moves pitch (up to +-12 semitones), -64 to +64; 0 = not at all, negative inverts. Not p-lockable. |
| `macro2_cutoff_depth` | CUT | Macro 2 to cutoff | How far macro 2 moves the filter cutoff, -64 to +64; 0 = not at all, negative inverts. Not p-lockable. |
| `macro2_res_depth` | RES | Macro 2 to resonance | How far macro 2 moves the filter resonance, -64 to +64; 0 = not at all, negative inverts. Not p-lockable. |
| `macro2_amp_depth` | AMP | Macro 2 to amp | How far macro 2 moves the track's level, -64 to +64; 0 = not at all, negative inverts. Not p-lockable. |
| `macro2_pan_depth` | PAN | Macro 2 to pan | How far macro 2 moves the pan position, -64 to +64; 0 = not at all, negative inverts. Not p-lockable. |
| `macro3_pitch_depth` | PTCH | Macro 3 to pitch | How far macro 3 moves pitch (up to +-12 semitones), -64 to +64; 0 = not at all, negative inverts. Not p-lockable. |
| `macro3_cutoff_depth` | CUT | Macro 3 to cutoff | How far macro 3 moves the filter cutoff, -64 to +64; 0 = not at all, negative inverts. Not p-lockable. |
| `macro3_res_depth` | RES | Macro 3 to resonance | How far macro 3 moves the filter resonance, -64 to +64; 0 = not at all, negative inverts. Not p-lockable. |
| `macro3_amp_depth` | AMP | Macro 3 to amp | How far macro 3 moves the track's level, -64 to +64; 0 = not at all, negative inverts. Not p-lockable. |
| `macro3_pan_depth` | PAN | Macro 3 to pan | How far macro 3 moves the pan position, -64 to +64; 0 = not at all, negative inverts. Not p-lockable. |
| `macro4_pitch_depth` | PTCH | Macro 4 to pitch | How far macro 4 moves pitch (up to +-12 semitones), -64 to +64; 0 = not at all, negative inverts. Not p-lockable. |
| `macro4_cutoff_depth` | CUT | Macro 4 to cutoff | How far macro 4 moves the filter cutoff, -64 to +64; 0 = not at all, negative inverts. Not p-lockable. |
| `macro4_res_depth` | RES | Macro 4 to resonance | How far macro 4 moves the filter resonance, -64 to +64; 0 = not at all, negative inverts. Not p-lockable. |
| `macro4_amp_depth` | AMP | Macro 4 to amp | How far macro 4 moves the track's level, -64 to +64; 0 = not at all, negative inverts. Not p-lockable. |
| `macro4_pan_depth` | PAN | Macro 4 to pan | How far macro 4 moves the pan position, -64 to +64; 0 = not at all, negative inverts. Not p-lockable. |

### MST: The MST upper key: the mixer, the master settings (tempo, swing, global length, playhead return) and the A / B scene crossfader. Setup: return mode, tempo, global BPM, project name.

| Item | Label | Name | What it does |
|---|---|---|---|
| `mute` | MUTE | Mute | Silences the track (its sends still feed S1 / S2). Saved with the pattern; also a held track key. |
| `global.glen` | GLEN | Global length | The global pattern length, 1 to 256 steps: the boundary that SEQ pattern changes (and TEMP returns) wait for. |
| `global.rtn` | RTN | Playhead return | What the playhead does when loop keys are let go: RETURN (back in sync), BOOMR (plays back out in reverse) or RESET (restart). |
| `e5.xfd` | XFD | Scene crossfader | The A / B scene fader, 0 (scene A) to 128 (scene B); morphs every param stored in the scenes. |
| `e5.gbpm` | GBPM | Global BPM | ON: one tempo for every pattern. OFF: each pattern keeps its own BPM. |
| `e5.name` | NAME | Project name | Turn to name the project (OCT+ keeps it, OCT- cancels). |

### FILE: The SAVE button: the sample browser and the slot's file settings, and RAZOR, the slice editor for the track's sample.

| Item | Label | Name | What it does |
|---|---|---|---|
| `slot.bpm` | BPM | Sample BPM | The tempo the sample was played at, 20 to 300; changing it recalculates its length in steps. Tempo-fitting warp modes use it. |
| `slot.steps` | STEP | Sample steps | How many sixteenth steps the trimmed sample spans, 1 to 512: the length tempo-fitting warp modes stretch it to. |
| `slot.gain` | GAIN | Sample gain | The slot's level, 0 to 4x, times each track's volume. |
| `slot.trim_start` | T-ST | Trim start | Where the sample starts, shown in seconds; everything (Range, loop, slices) lives inside the trim. SHIFT = finest step. |
| `slot.trim_end` | T-EN | Trim end | Where the sample ends, shown in seconds. SHIFT = finest step. |
| `razor.start` | STRT | Slice start | RAZOR: the selected slice's start, 0 to 1 of the track's window, moved under the SNAP mode. |
| `razor.end` | END | Slice end | RAZOR: the selected slice's end, 0 to 1 of the track's window, moved under the SNAP mode. |
| `razor.play` | PLAY | Slice play mode | RAZOR: the selected slice's own play mode, or TRACK to use the track's. |
| `razor.zoom` | ZOOM | Zoom | RAZOR: zooms the waveform 1x to 64x, centred on the point last moved. View only. |
| `razor.snap` | SNAP | Snap mode | RAZOR: how STRT / END move - GRID (slice-count divisions), ZOOM (fine, by zoom), ZERO-X (zero crossings), TRANS (transients), FRIEND (drags the touching neighbour too). |
| `razor.choke` | CHOK | Slice choke group | RAZOR: the selected slice's choke group, A to H or OFF; a new voice in the group cuts the others. |
| `razor.pitch` | P/T | Slice pitch | RAZOR: the selected slice's pitch offset, -24 to +24 semitones. |
| `razor.sens` | SENS | Transient sensitivity | RAZOR: sensitivity of the transient detection (the ticks) used by Auto-Chop and TRANS snap, 0 to 100. |

### PAT: The ARP button: the track's pattern length and step rate, the global length and how pattern changes engage. Setup: change mode, global length.

| Item | Label | Name | What it does |
|---|---|---|---|
| `pattern_steps` | LEN | Pattern length | The track's pattern length, 1 to 256 steps. Tracks can differ (polymeter). |
| `pattern_rate` | RATE | Pattern rate | The track's step speed against the tempo, 1/16 to 2 (1 = sixteenth notes; 2 = twice as fast). |
| `global.quant` | QUANT | Pattern change mode | How a picked pattern engages: SEQ (at the global length boundary), JUMP (now, same step), START (now, from step 1), TEMP (now, back after one pass). |

### OTHER

| Item | Label | Name | What it does |
|---|---|---|---|
| `lfo1_dest` | DEST | LFO 1 destination | What LFO 1 moves: OFF, PITCH (+-12 st), CUT, RES, AMP, PAN or a macro (M1-M4). Not p-lockable. |
| `lfo2_dest` | DEST | LFO 2 destination | What LFO 2 moves: OFF, PITCH (+-12 st), CUT, RES, AMP, PAN or a macro (M1-M4). Not p-lockable. |
| `menv_dest` | DEST | Mod env destination | What the mod envelope moves: OFF, PITCH, CUT, RES, AMP, PAN or a macro (M1-M4). Not p-lockable. |
| `mvel_dest` | DEST | Velocity destination | What velocity moves: OFF, PITCH, CUT, RES, AMP, PAN or a macro (M1-M4). Velocity comes from slice trigs only. Not p-lockable. |

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
| SRC/LOOP | The main view: the track's waveform with pitch, loop start / end and sample slot (slice machines: play mode, slice, slice count, choke, slice mute). Empty for NEIGH and INPUT, which show their routing / input meters. |
| SRC/MACHINE | The machine's own controls: loop reverse, crossfade and trig release for loop machines; slice, play mode, reverse, ratchet, clock sync and rate for slice machines. |
| SRC/WARP | The warp mode's controls (each mode shows its own: grains, stretch, harmonies, wavetable ...) plus RATE, over the waveform. Empty for NEIGH and INPUT. |
| SRC/RANGE | The Range: a narrower window inside the sample's trim that the loop points, loop keys and slices work within. |
| SRC/SETUP | The track's machine, warp mode (changes only while stopped), loop-key division and trig polyphony. |
| FILT/FILTER | The filter machine's controls (cutoff, resonance and its own third / fourth knob) and the filter envelope depth, over the filter curve. |
| FILT/ENV | The filter envelope's times (ADSR or AHR, per the setup) and its depth onto the cutoff. |
| FILT/MIX | How the filter treats the two channels: the stereo / mid-side cutoff spread (BAL) and the dry / wet mix (COMB and FORMNT only). |
| FILT/SETUP | The filter machine, the filter envelope mode and the stereo / mid-side mix mode. Machine and mix mode change only while stopped. |
| AMP/ENV | The amp envelope (ADSR or AHR), track volume, pan and the S1 / S2 send levels, over the envelope's shape. |
| AMP/SETUP | Amp envelope mode, the time range of every envelope, portamento (legato), and how long slice notes last. |
| FX/FX | The track's insert effect: the chosen FX machine's knobs over a picture of it (delay taps, reverb tail, compressor curve with meters ...). Empty for NONE. |
| FX/SENDS | How much of the track goes to the S1 and S2 send buses, and where it is tapped (before or after the insert). |
| FX/SETUP | Choose the insert FX machine (any time, with a short fade) and the send tap point. |
| MOD/LFO 1 | LFO 1: destination, waveform, synced speed, bipolar depth and trigger mode. The bar shows the live value at the destination. |
| MOD/LFO 2 | LFO 2: destination, waveform, synced speed, bipolar depth and trigger mode. The bar shows the live value at the destination. |
| MOD/ENV | The mod envelope (ADSR), its destination and depth. It retriggers on steps whose ERST is on, for the note's length. |
| MOD/VELOCITY | Velocity as a mod source: where it goes and how much. Velocity comes from slice trigs only. |
| MOD/MACROS | The four macro values M1-M4 at a glance. Each macro moves pitch, cutoff, resonance, amp and pan by its own depths. |
| MOD/MACRO 1 | Macro 1: its value and how far it moves pitch, cutoff, resonance, amp and pan. |
| MOD/MACRO 2 | Macro 2: its value and how far it moves pitch, cutoff, resonance, amp and pan. |
| MOD/MACRO 3 | Macro 3: its value and how far it moves pitch, cutoff, resonance, amp and pan. |
| MOD/MACRO 4 | Macro 4: its value and how far it moves pitch, cutoff, resonance, amp and pan. |
| MST/TRACK MIX | The mixer: all six tracks' levels and the S1 / S2 returns at a glance; the knobs set the selected track's volume, pan and mute, and the tempo. |
| MST/MASTER | Project timing: tempo, swing, global pattern length and what the playhead does when the loop keys are let go. |
| MST/SCENE | The A / B scene crossfader: the fader, its glide positions and the params that morph. The lower keys are the scene row here. |
| MST/SETUP | Playhead return mode, tempo, global BPM (one tempo for every pattern) and the project's name. |
| FILE/SAMPLE | The sample browser (SELECT moves the cursor, SAVE loads into the track's slot) and the slot's BPM, length in steps, gain, slot number and trim. |
| FILE/RAZOR | The razor slice editor for the track's sample: move each slice's start / end with a snap mode, zoom, set its play mode, choke group, pitch and the transient sensitivity. OCT+ splits, OCT- deletes. |
| PAT/PATTERN | The pattern grid with the track's pattern length and step rate, the global length and the pattern change mode. |
| PAT/SETUP | How a newly picked pattern engages (quantize mode) and the global length it waits for. |
| bus:S1 | Send bus 1's effect: the knobs of the FX machine on the S1 return (global, not p-lockable; morphs with the scenes). |
| bus:S1/SETUP | Choose the FX machine on the S1 return. NONE returns nothing. |
| bus:S2 | Send bus 2's effect: the knobs of the FX machine on the S2 return (global, not p-lockable; morphs with the scenes). |
| bus:S2/SETUP | Choose the FX machine on the S2 return. NONE returns nothing. |
| bus:MST | The master insert: the knobs of the FX machine on the final mix, send returns included (global, not p-lockable). |
| bus:MST/SETUP | Choose the FX machine on the master insert. NONE passes the mix through clean. |
| bus:MST/SRC | With the master bus selected, SRC shows the mixer: track levels and the returns, with the selected track's volume, pan, mute and the tempo. |
| F3 | The timing pop-up (OCT- + OCT+, or PLAY held): tempo on SELECT, swing on ALGORITHM, the track's pattern length and rate, global length and change mode; the lower keys pick and loop the step pages. |

<!-- params:end -->

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
