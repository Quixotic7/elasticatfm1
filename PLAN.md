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
   the hosts is a separate project.

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

Samples are stored as **int16 stereo** in a fixed arena (128 MiB memory ≈ 12 min of stereo audio at 44.1 kHz), read
whole at load (loops are short; streaming per VIRTUAL-CARTS §5 only if needed later). Other sample rates are played
at the right speed by the reader (ratio), not resampled at load.

Project save: the flash image (patterns, parameters, sample *paths*), versioned record, so it follows RemoteFM1's
states. A missing sample shows "missing: /audio/x.wav" and plays on.

## Panel mapping (from the mockup)

- **MASTER pot**: master volume. **SELECT**: page. **PRESETS knob**: pattern. **ALGORITHM knob**: track.
  **KN1-KN4**: the page's cells 1-4 (cells 5-8 with HOME/FN held).
- **Buttons**: FX→FX · SEL→SRC · ENV→AMP · LFO→MOD · EDIT→FILT · GLO→MASTER · HOME→FN (shift) · SAVE→FILE ·
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
- **E6** (separate project): audio input in the core ABI and hosts; REC page and the input machine.

## How the work is done

Opus agents write the code from briefs; Claude designs, briefs, reviews and tests. File ownership by directory so
agents run in parallel. Nothing is pushed without the owner's OK.
