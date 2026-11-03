# Elasticat FM-1: plan

Elasticat (https://github.com/Quixotic7/Elasticat, GPL-3.0, norns + grid, Lua + SuperCollider) ported to the FM-1 as a
**Fantasy CFW**: a host-only `.rfm1` that runs in RemoteFM1 hosts (remotefm1.com web player first, then the iOS and
Android apps, FM1VST later), with the FM-1 as its panel. Samples come from the shared RemoteFM1 folder (`/audio`).

Design source: the owner's canvas "Elasticat for iOS, macOS & FM-1"
(https://claude.ai/artifact/HGE3JSbQiEy658QTE3KbXd), artboards **FM-1 · Proposed panel mapping** and
**FM-1 · 240×240 screens**; local copies in `design/mockup/` (FM1Panel, FM1Screens, Display, Main, Razor).

## Decisions (owner, 2026-11-02)

1. **All C.** The sequencer, parameter model, UI and DSP are rewritten in C; the Lua script and the SuperCollider
   engine (`../Elasticat/`, read-only reference) are the spec. Fast in wasm3, and the same C can later go into a native
   iOS app.
2. **Core loop first** (E1-E2 below), then the rest in milestones.
3. **No audio input yet**: no REC page, no `input` machine. The core ABI (`fm1core.h`) has no audio-in; adding it to
   the hosts is a separate project. (2026-11-02: E6 below, on the `.rfm1` capability `ain1`.)

## Platform facts

- Core ABI: `FM1VST/core-api/fm1core.h` (ABI 6). 44.1 kHz, blocks of 128 frames, s16 stereo out; 240x240 RGB565 LCD;
  27 keys (11 upper / sharps, 16 lower / naturals) with LEDs; 14 buttons; SELECT + K1..K4 encoders (+ others by role)
  and the MASTER pot.
- Fantasy CFW rules: `FM1VST/.claude/worktrees/rfm1-manager/docs/VIRTUAL-CARTS.md` (on branch `rfm1-manager`). File
  API `rfm1_host_1` (async `fs_request` / `fs_poll`, 1 MiB per request, 32 in flight), memory fixed ≤ 512 MiB,
  flash image ≤ 4 MiB (the save), `rfm1_run` must show ≥ 2x real time in wasm3 on an M-series Mac.
- Build: FM1VST's `fm1_add_core` + `fm1_add_rfm1(elasticat VIRTUAL NEEDS fs1 FOLDERS audio=r data=rw MEMORY_MIB 128)`;
  model glue: `core-glue/ripple/` (the other Fantasy CFW). Our glue lives in an FM1VST worktree on branch
  `rfm1-elasticat` (from `rfm1-manager`), with this repo as submodule `cores/elasticatfm1`.

## Architecture (this repo)

```
src/dsp/      the sound engine: sample store, voices, warp readers, filters, amp, mixer. Pure C99, no platform, no
              malloc in the audio path. Header: src/dsp/ec_dsp.h
src/seq/      project model + sequencer: tracks, patterns, steps, trigs, p-locks, trig conditions, transport/clock,
              live loop-key scrub, the parameter spec (categories/pages/cells) and value resolver
src/ui/       240x240 RGB565 renderer (pixel font of the mockup), screens, panel mapping, LEDs
src/io/       sample loading over the file API (WAV/AIFF parse -> int16 in the sample store), folder browsing,
              project save/load in the flash image
src/plat/     fm1core glue: fm1core_get, panel, tick/frame/audio entry points, rfm1_host_1 imports (and a native
              stand-in that reads a real folder, for tests)
host/         native harness: run headless with scripted input, write WAV + PNG screens; unit tests (ctest)
design/       mockup copies, notes
```

Samples are stored as **int16 stereo** in a fixed arena (96 MiB of the 128 MiB ≈ 9.5 min of stereo at 44.1 kHz), read
whole at load (loops are short; streaming per VIRTUAL-CARTS §5 only if needed later). Other sample rates are played
at the right speed by the reader (ratio), not resampled at load.

Project save: the flash image (patterns, parameters, sample *paths*), versioned record, so it follows RemoteFM1's
states. A missing sample shows "missing: /audio/x.wav" and plays on.

## Panel mapping (from the mockup)

- **MASTER pot**: master volume. **SELECT**: page. **PRESETS knob**: pattern. **ALGORITHM knob**: track.
  **KN1-KN4**: the page's cells 1-4 (cells 5-8 with GLO/FN held).
- **Buttons**: FX→FX · SEL→SRC · ENV→AMP · LFO→MOD · EDIT→FILT · GLO→FN (shift) · HOME→MASTER · SAVE→FILE ·
  ARP→PAT · SEQ→TRIG · PLAY→PLAY · REC→REC (reserved: copy/clear/paste scope key as in Elasticat). OCT-/OCT+: key
  mode.
- **Upper keys (11)**: T1..T6, S1, S2, MST, FILL, PATN. LED: selected track bright, others dim by activity.
- **Lower keys (16)**: steps 1-16 (LED: trig orange, playhead cream). Hold SEL = loop keys 1-16 (slice pads for slice
  machines). Hold a step + turn a knob = p-lock.
- **Screens**: SRC/WARP (waveform, region, playhead, loop-key ruler, 4 cells), FILE (sample browser, later RAZOR),
  MASTER/MIX (meters + faders, upper keys pick a strip), and one page per category with the 4-cell grid.
  Look: black ground, cream greys (#F6F3E8 / #CFCBBE / #9A978B / #5E5C54 / #34332E), one orange accent #FF6A1F,
  the 3x5 pixel font at 2x (the mockup's 120x120 logical grid at scale 2).

## Milestones

- **E0 pipe**: repo skeleton, CMake, native harness, fm1core glue, FM1VST glue + `.rfm1` that passes `rfm1_check`, loads
  a WAV from `/audio` via the file API and loops it, screen shows the title and the file, keys/knobs echo. Runs in
  `rfm1_run --folder` and in the web player.
- **E1 core loop**: 6 tracks; machines loop, loop_trig; warps tape, tempo_varispeed; amp; transport + clock (BPM);
  16-step trigs per track; live loop-key scrub; FILE sample browser; SRC/WARP screen; LEDs.
- **E2 playable**: p-locks; grid_slice + slice_poly; granular warp; filter machines (classic, ladder, svf); MIX screen;
  16 patterns; save/load in the flash image; trig conditions + FILL.
- **E3**: razor_slice / razor_poly + the razor screen; the other warp modes; neighbor machine.
- **E4**: FX machines, sends S1/S2, master insert, MOD (LFOs).
- **E5**: scenes A/B + crossfader, copy/clear/paste, page modes, memorize/recall.
- **E6**: audio input (the `.rfm1` capability `ain1`, FM1VST docs/VIRTUAL-CARTS.md 8; the native ABI later), the
  INPUT machine, REC (sampling).

## Status (0.6.0, 2026-11-03)

- **E0-E2** done (0.1.0): the pipe, the core loop, the playable instrument; web player e2e.
- **E3** done: RAZOR / R.POLY + the FILE / RAZOR page (Auto-Chop, snaps, split / delete, per-slice settings), every
  warp mode (domino, random_ola, pitch_corrected, harmonizer, wavetable, spectral_freeze, formant, gstretch,
  gstretch2; real FFT), the NEIGH machine.
- **E4** done: 18 FX machines on the inserts, S1 / S2 and the master insert; sends PRE / POST; MOD (2 LFOs, mod env,
  velocity, 4 macros + matrix); the idle CPU saver.
- **E5** done: scenes A / B + crossfader and glide (tracks and buses), copy / clear / paste per scope, PAGE overlay
  (select / loop / rate), memorize / recall, undo, pattern / project names, slice programs, per-pattern BPM, pads 17-32.
- **Integration** (0.2.0): e3 + e4 + e5 merged into main; resolutions in the merge commits and docs/FM1-CONTROLS.md
  (FN + SAVE = undo; SAVE on SAMPLE opens RAZOR for the track's own file; OCT keys are the razor's only on RAZOR).
  Tests: ctest 14 (incl. test_v020: the seams through the real chain, and the v020 demo + reboot), make -C tests/dsp
  (66 + 43 + 262 + 28), clang and emcc -Wall -Wextra clean.
- **UX redesign** (0.3.0): the panel along ChoralRoot and FIZZ (docs/UX-REDESIGN.md, the owner's decisions;
  docs/FM1-CONTROLS.md "What moved in 0.3.0"): SHIFT on GLO (snap, shortcuts, its map), lanes of four stepped by
  SELECT, setup by a 0.4 s hold or SHIFT, HOME back to SRC, the MST key = the MST category, PRESETS = the track's
  sample (debounced), F3 (OCT- + OCT+ or PLAY held), pop-ups with OCT- back / OCT+ OK, redo, knob acceleration,
  the held-step frame + lock chips, OCT+ held + a knob clears a lock, track keys hold = mute / double-tap = solo,
  the knob takeover, the live panel labels (ec_app_labels). Tests: ctest 16 (test_ux, the ux demo; every demo's
  gestures updated), the web e2e with a live-label check. CPU: the heavy project 2.4x in wasm3.
- **E6** (0.4.0) done, **hosts pending** (no RemoteFM1 host feeds input yet; it runs without): the API decided by
  the RemoteFM1 session, capability **ain1** (FM1VST docs/VIRTUAL-CARTS.md 8): manifest `"wants": ["ain1"]`, the
  optional exports `rfm1_audio_in(stereo, frames)` (immediately before `rfm1_audio`, the same frames) and
  `rfm1_audio_in_info(state, latency_frames)`; FM1VST builds it (`fm1_add_rfm1(... WANTS ain1)`), checks it
  (rfm1_check) and runs it (`rfm1_run --audio-in`). Here: src/plat keeps the block and hands it to the same audio call
  (ec_plat_ain, the state and latency), the engine's INPUT machine (ec_dsp_input: frame i in = frame i out, the
  track's filter / FX / amp / sends), the REC pop-up (src/app/ec_rec.c, src/ui/ec_scr_rec.c: IN or MST, a pool slot,
  LEN free / 1-16 bars, THR, SYNC PLAY / BAR / FREE, the latency shift (the host's latency_frames = the round
  trip, decided for every host), the take into the slot at once and the WAV to
  /data/rec over FS_WRITE in 1 MiB pieces from the tick), host/ec_run --audio-in. Tests: test_e6 (48 checks: the
  INPUT alignment to the frame, state 0 silent, the pop-up's gestures and labels, a take synced on PLAY of a click track
  looped back through a 300-frame latency on the output's own frame, a resample, the threshold arm to the frame, the WAV
  bytes and the reboot reloading it), the e6 demo (results/e6), the glue's `rfm1_elasticat` (input fed, the pop-up
  armed) and `rfm1_elasticat_ain_off`; the web e2e passes on RemoteFM1 main (no input: skipped)
  and, with a fake microphone, on fw-catalog c5866b9 (the web player's ain1: latency reported 1419-1420 frames; the
  INPUT track sounds, a FREE take lands and saves). Not checkable headless: a synced take's alignment through a real
  speaker-to-mic loop (the fake mic is a file, not a loopback); the native test covers it. Max take: the free arena (~9.5 min of
  stereo with an empty pool). CPU: the heavy project 2.3x in wasm3, with or without the input fed.
- **Memory** (wasm, MEMORY_MIB 128): engine 17 523 904 bytes (EC_DSP_MEM_MAX 18 MiB, checked at compile time),
  sample arena 96 MiB (~9.5 min of stereo 44.1 kHz), the app state 2.5 MiB + the memorize copy 2.3 MiB, the flash
  buffers and host shim ~3.4 MiB, the stack 1 MiB: the stack top is at 123.1 MiB, 4.9 MiB free. The arena stays 96 MiB.
- **CPU** (rfm1_run, wasm3, M-series): the heaviest project (`ec_make_heavy`: six busy tracks in harmonizer /
  random_ola / spectral freeze / R.POLY x8 / wavetable / gstretch2, six inserts incl. BLACKHOLE, RINGS, REVERB, both
  sends, master COMP, LFOs on every track) 2.1x real time while playing; the v020 demo project 5.9x.

- **Mod matrix** (0.5.0, docs/MOD-MATRIX.md): 8 slots of SRC / DST / DEPTH per track (LFO 1 / 2, ENV, VEL,
  aftertouch, CC 1, CC 11, M1-M4 onto any continuous param; PITCH / CUT / RES / AMP / PAN on the fast buses, the rest
  once per block in the sequencer), the MOD / MATRIX page, MOD KEYS (hold MOD or SHIFT + MOD: the black keys are the
  sources), the cells' marks, 0.4 projects converted on load at the same strength. Tests: ctest 23 (+ mtx, 68
  checks), the DSP suites, the designer (56), the web e2e (18). CPU: the heavy project with every slot full
  (generic destinations on every track) 2.3x in wasm3 (its chord section 2.1x).

- **0.6.0 modes** (2026-11-03, docs/UX-V2.md, built as specified; the build's own decisions in its §8): the panel
  by modes (FX, FILTER, AMP, MODULATION, MIX / the CAT / PAGES, LOOP ENGINE / TRIG / KEYBOARD, FILE / SAMPLER,
  CROSSFADER, PATTERN LAUNCH), pages remembered per mode, settings by a hold or SHIFT, the mode pop-ups, sticky SHIFT
  and its layer, F1 / F2 copy / cut / paste, the source template (B6-B10 route through the matrix; MOD KEYS gone),
  APP SETTINGS in its own flash record (six 16-colour themes), live REC (capture / fixed / overdub, count-in, the
  click after the master, micro timing, automation as lock-only steps), FILE's folder browser / preview / LOAD into
  the next free slot / pool meter / editor / purge, the bar-synced SAMPLER, 64 patterns (4 banks) and 128 slots
  (record version 2, sparse: a dense 64-pattern project ~275 KB, the image stays 1 MiB; 0.5 projects load into bank
  A), the arena 219 MiB at MEMORY_MIB 256, live grains on the GRAIN warp, slot locks, every screen on a 120x120
  16-colour grid at 2x (norns bar renders, the intro logo, the CAT). Tests: ctest 28 (new: grid, ux2_real, rec, file,
  liverec; test_ux 204, mtx 62, liverec 66, rec 69, file 90, e6 49 checks; grid 4848 frames x 6 themes), the DSP
  suites + test_v6 (74), the designer (67), the web e2e (20, RemoteFM1 main with ain1). CPU: the heavy project 2.3x
  real time in wasm3 at 256 MiB (its chord section 2.1x). The real screens vs the mockups: host/render_ux2_real.sh ->
  results/ux-v2-real/.

Remaining / open:
- **E6 in the hosts**: the web player first (then iOS incl. the AUv3 input bus, Android, FM1VST); the native ABI's
  descriptor fields at the next bump. Untried with a real microphone. The REC settings are runtime only; a take with
  no folder is not saved (it plays until the project reloads).
- The bus lanes (p-locks of the S1 / S2 / MST params) are not ported; bus params are global (they do morph).
- Two slice stores coexist (the razor table per track, the pattern's slice program); merging them into one would
  change the save format. Hold-anchor + turn on a bus page does not write the bus param into a scene (capture does).
- CPU headroom for the worst case is thin (2.1x); the next CPU work would be the spectral / harmonizer stages.

## How the work is done

Opus agents write the code from briefs; Claude designs, briefs, reviews and tests. File ownership by directory so
agents run in parallel. Nothing is pushed without the owner's OK.
