# Elasticat FM-1

Elasticat, the norns sample-mangling sequencer ([Quixotic7/Elasticat](https://github.com/Quixotic7/Elasticat),
GPL-3.0), rewritten in C for the M-VAVE FM-1 as a **Fantasy CFW**: a host-only `.rfm1` that runs in RemoteFM1 hosts
(the remotefm1.com web player first), with the FM-1 as its panel and samples from the shared RemoteFM1 folder
(`RemoteFM1/audio/`). The plan, panel mapping and milestones are in [PLAN.md](PLAN.md).

**User guide: [quixotic7.github.io/elasticatfm1](https://quixotic7.github.io/elasticatfm1/)** (source in
[docs/guide/](docs/guide/index.html), published by GitHub Pages on every push to `main`).

**Status: 0.6.1, the panel by modes** ([docs/UX-V2.md](docs/UX-V2.md); 0.6.1: projects are files in `/data/projects/`, two inserts per track, a counted load meter on FX's INSERT page, no pop-ups, messages and held keys in the header, the owner's play-test fixes, UX-V2 sections 9-11): each button opens a mode (FX, FILTER, AMP,
MODULATION, MIX / PAGES, LOOP ENGINE / TRIG / KEYBOARD, FILE / SAMPLER, CROSSFADER, PATTERN LAUNCH) with its pages,
the white keys are steps, the black keys the mode's pages and the modulation sources, SHIFT a layer, F1 / F2 copy /
cut / paste, REC live recording, 64 patterns and 128 pool slots, and every screen on a 120x120 16-colour grid shown
at 2x (six themes). Six tracks, each with a sample from `/audio`, a machine (loop, loop_trig, grid_slice,
slice_poly, razor_slice, razor_poly with the RAZOR slice editor, neighbor, input), one of thirteen warp modes, a filter
machine (classic, morphing, ladder, comb, formant, svf), envelopes, level and pan, two insert FX in series (18 machines each), sends
to S1 / S2 and MOD (2 LFOs, mod env, velocity, macros); the master insert; 16 patterns of up to 256 steps per track
with trigs, ghost trigs, p-locks of any param, trig conditions, chance, ratchets and FILL; scenes A / B with a
crossfader and glides; copy / clear / paste, the PAGE overlay, memorize / recall, undo, names; the live loop-key
scrub and slice pads; sample-accurate clock; the project saved as a file in `/data/projects/` (the flash keeps the app settings and its name). E6 (0.4.0): the host's live
audio input (capability `ain1`: no RemoteFM1 host feeds it yet, the cart runs without) as the **INPUT** machine, and
**REC** sampling of the input or the master into a pool slot (synced on PLAY / the bar / a threshold, latency
compensated, saved as a WAV in `/data/rec`). The panel: [docs/FM1-CONTROLS.md](docs/FM1-CONTROLS.md). With no folder (or no audio files) a built-in loop plays on T1.

## Layout

```
src/plat/   fm1core ABI glue (ec_core.c: fm1core_get, the panel, LEDs, LCD, the 1 MiB flash image with its versioned
            header), ec_plat.h (what the rest of the code sees), abi/ (copies of FM1VST's fm1core.h and rfm1_host.h
            for the native build)
src/seq/    the model and the sequencer, platform free: ec_params (params_spec.lua: ids, ranges, labels, the xforms to
            ec_track_params_t), ec_project (16 patterns x 6 tracks of params + steps + p-locks, the slot pool, the
            versioned project record: the patterns, then tagged chunks RZR1 / EC4B / E5), ec_seq (transport, sample
            clock, trig conditions, holds = the base-value resolver, region locks, ratchets, pattern switching, live
            loop keys / pads / keyboard), ec_razor (E3: razor tables, Auto-Chop), ec_params_e4 (E4: FX + MOD params,
            the bus records), ec_scene + ec_e5seq (E5: scenes, clipboards, page loops, undo, slice programs)
src/app/    ec_app (the panel mapping, sample loading, save / autosave, LEDs, ui_info), ec_pages (categories,
            pages, cells, setup lists, read from the layout: ec_layout.h; ec_layout_gen.c is GENERATED from
            design/layout/elasticat.json by tools/layout/gen_layout.py), ec_razor_ui (E3: the RAZOR page), ec_e5 (E5: scenes, PAGE, memorize,
            undo, naming), ec_ux (0.3.0: lanes, holds, labels), ec_rec (E6: the REC pop-up, the take, the WAV writer)
src/io/     ec_fs (async file API over rfm1_host_1), ec_fs_native (a native stand-in serving a real folder, one tick
            late), ec_browse (recursive /audio scan), ec_audiofile (WAV / AIFF / AIFC parse + decode), ec_loader
            (chunked loading), ec_samplestore (int16 stereo arena + handles, bpm from file names)
src/ui/     ec_gfx (RGB565, palette, rects / lines / dots, the mockup's 3x5 pixel font), ec_leds, ec_screens (SRC,
            generic pages with the step strip / filter curve / envelope, FILE, MIX, PAT, setup lists), ec_scr_e3 /
            e4 / e5 (RAZOR + WARP + NEIGH, FX + MOD, SCENE + PAGE + naming + toasts)
src/dsp/    the sound engine (ec_dsp.h; E3 warp modes ec_warp3 + ec_fft, E4 ec_fx + ec_mod; src/dsp/README.md).
            ec_dsp_mem_size() = 16.7 MiB, EC_DSP_MEM_MAX 18 MiB (a compile-time check)
host/       ec_run (headless native harness: scripted input, WAV + PNG out), test_io (the file layer's test), test_seq
            (the sequencer's), make_testloop.py (the 4 s test loop), make_demo_folder.py + demo_e1.txt + render_e1.sh
            (the E1 demo built through panel input only), web_e2e.mjs (the .rfm1 in the real RemoteFM1 web player);
            E3: test_e3, demo_e3.txt + render_e3.sh; E4: test_seq_e4, demo_e4.txt + render_e4.sh; E5: test_e5
            (scenes, glide, clipboards, page loop, BPM, memorize, undo, slices, the record + an E2 save in
            fixtures/e2_flash.bin), demo_e5.txt + render_e5.sh; 0.2.0: test_v020 (the E3 / E4 / E5 seams through
            the real chain), demo_v020.txt + render_v020.sh (+ a reboot), make_heavy.c (the CPU worst case);
            0.3.0: test_ux (the panel's gestures and live labels through the core), demo_ux.txt + render_ux.sh;
            E6: test_e6 (input alignment, sampling, the WAV + reboot), demo_e6.txt + render_e6.sh; the layout:
            ec_layout_test_gen.c (the tests' fixed layout, 0.4.0's pages, from fixtures/layout-0.4.0.json: the
            tests and demo scripts run on it, ec_run --layout test), dump_pages.c (every page in every context,
            hashed: the fixed layout = 0.4.0's hand-written pages)
design/layout/  elasticat.json: the layout (pages, lanes, knobs, setups, buses, F3), compiled into the build
tools/layout/   gen_layout.py: the layout's checks and generator (C tables, docs/FM1-CONTROLS.md's page tables)
results/    proof files: e0/ .. e6/, v020/, ux/ (the screens; WAVs stay untracked), web/ (the web player test's
            screenshots + SUMMARY.txt)
```

## The APIs, briefly

- **`ec_plat.h`:** buttons by label (`EC_B_PLAY`, `EC_B_SAVE`, ..; Elasticat's names in the comments), knobs by role
  (`EC_E_SELECT`, `EC_E_K1`..), the keys as rows (`ec_key_lower/upper`, `ec_key_of_lower/upper`). One `ec_input_t`
  per device ms (held / down / up masks, velocities, detents by role, MASTER 0..1023). LEDs: `ec_plat_led_lower /
  upper / button / play_green`. The LCD: `ec_plat_lcd()` (big-endian RGB565) + `ec_plat_lcd_commit()`. The flash
  record: `ec_plat_record()`. The app implements `ec_app_init / tick / frame / audio / ui_info / shutdown`. E6: the
  host's input, `ec_plat_ain(&n)` (inside `ec_app_audio` only: this very block's input, frame i = output frame i, or
  NULL), `ec_plat_ain_state()` (0 none, 1 live), `ec_plat_ain_latency()` (frames); the .rfm1's `rfm1_audio_in` /
  `rfm1_audio_in_info` reach it as `fm1core_audio_in` / `fm1core_audio_in_info` (ec_core.c), which the native
  harness calls directly. The engine: `ec_dsp_input(dsp, lr, frames)` before rendering = the INPUT machine's source.
- **io:** `ec_fs_start / ec_fs_update / ec_fs_cancel` on a caller-owned `ec_fsreq_t` (never blocks, at most 32 in
  flight, at most 1 MiB per request). `ec_browse_start("/audio")` + `ec_browse_step()` per tick. `ec_load_start(path)`
  + `ec_load_step()` per tick: STAT, header reads, then data in ≤ 1 MiB reads, decoded 4096 frames per tick into the
  store. `ec_ss_alloc / get / ready / reset`: int16 stereo (mono doubled) in a 200 MiB static arena (0.6.1), with rate,
  channels, bpm (`ec_bpm_from_name`, Elasticat's rule) and a 128-column overview.
- **ui:** `ec_gfx_begin(lcd)`, `ec_gfx_rect / line / dots_h / text_al` in physical pixels; the mockup's 120x120 grid
  is scale `EC_U` = 2. Palette `EC_C_BG, L3, L5, L8, L12, L15, ACC`. `ec_screens_draw(app)`.
- **seq:** `ec_seq_init(seq, project, sink, fs)`; `ec_seq_render(seq, out, frames)` splits the frames at every due
  moment and drives the engine through the sink (`event` / `params` / `process`); `ec_seq_start / pause / stop /
  set_bpm / request_pattern / set_fill / loop_key / slice_pad / keyboard / touch`; `ec_seq_value(seq, t, id)` = the
  resolved value. Steps: `ec_step_toggle / toggle_ghost / toggle_slice / lock_set / lock_get`.

## Building and testing natively

```sh
cmake -S . -B build && cmake --build build -j8 && ctest --test-dir build --output-on-failure
build/ec_run --folder <RemoteFM1 root> --press PLAY@1000 --wav out.wav --png screen.png --png-scale 2
host/render_e1.sh build results/e1     # the E1 demo: screens + WAVs, reboot from the saved flash, a missing sample
host/render_v020.sh build results/v020 # the 0.2.0 seams demo + a reboot (render_e3 / e4 / e5.sh likewise)
host/render_ux.sh build results/ux     # 0.3.0: the new screens (lanes, setup, F3, SHIFT's map, locks, takeover)
host/render_e6.sh build results/e6     # E6: REC (idle, armed, taking, landed), the INPUT machine, NO INPUT, resample
make -C tests/dsp && make -C tests/dsp bench   # the engine's own tests (test_dsp, test_e3, test_fx, test_e4) + CPU
```

`ec_run` options are listed at the top of `host/ec_run.c`: `--press PLAY@MS[+D]`, `--turn SELECT@MS:+1`,
`--key 9@MS[+D][:VEL]`, `--lower I@MS[+D][:VEL]` / `--upper I@MS[+D]` (the step keys / the track row), `--master
512@MS`, `--png-at MS:file`, `--info-at MS`, `--expect-ui`, `--min-rms`, `--flash FILE` (the flash image across runs),
`--script FILE` (the same options one per line, see `host/demo_e1.txt`). E6: `--audio-in WAV` (a host's live input:
the WAV fed through `fm1core_audio_in` before each audio block, looped; `--audio-in-once`), `--audio-in-latency N`,
`--audio-in-off-at MS` (state 0), `--audio-in-on-at MS`.

## The layout (pages, lanes, knobs)

Which parameter is on which knob, lane, page and category is data: `design/layout/elasticat.json`, compiled into
`src/app/ec_layout_gen.c` by `tools/layout/gen_layout.py` (CMake re-runs it; the generated file is committed). The
designer, a web page whose preview is the firmware compiled to WebAssembly, rearranges it and exports the file:
`sh tools/layout/build_layout.sh`, then serve `build/layout-web` (`.claude/launch.json`: `elasticat-layout`,
http://127.0.0.1:8768/). How to hand a design back, what is fixed, the checks: [tools/layout/README.md](tools/layout/README.md).
The tests and demo scripts walk the pages by panel input, so they run on a fixed copy of 0.4.0's layout
(`host/fixtures/layout-0.4.0.json`, `ec_run --layout test`); `host/test_layout.c` (ctest `layout_walk`) checks the
shipped layout whatever its arrangement, finding every param by id through SELECT and the category buttons.

## The .rfm1

It is built by FM1VST from this repo (submodule `cores/elasticatfm1`, glue `core-glue/elasticat`, branch
`rfm1-elasticat`). See that glue's README.md. In short, in the FM1VST worktree:

```sh
cmake -S . -B build-rfm1 -DFM1_RFM1=ON -DFM1_RFM1_ONLY=ON -DFM1_ONLY_CORES=elasticat -DFM1_PUBLIC_BUILD=ON \
      -DFM1_EMCC=/opt/homebrew/bin/emcc -DCR_PYTHON=<RFM1Manager>/.venv/bin/python [-DEC_SOURCE=<this repo>]
cmake --build build-rfm1 -j8 --target rfm1_elasticat rfm1_run
ctest --test-dir build-rfm1 -R rfm1_elasticat -V        # rfm1_check + rfm1_run --folder core-glue/elasticat/test_folder
```

CPU in wasm3 (must stay >= 2x real time): `build/ec_make_heavy heavy.flash` writes the worst-case project for
`build/demo_folder` (six busy tracks across the E3 modes, every insert, both sends, master COMP, LFOs), then
`rfm1_run --folder build/demo_folder --flash-in heavy.flash --settle 3000 --seconds 10 --press PLAY@1500 <the .rfm1>`:
2.1x real time while playing in 0.2.0 (M-series), 2.4x in 0.3.0, 2.3x in 0.4.0, 2.3x in 0.6.0 at MEMORY_MIB 256 (chord 2.1x), 2.2x in 0.6.1 (chord 2.1x; every track with two heavy inserts, `ec_make_heavy out --ins1 8 --ins2 3`: 1.6x); 0.6.2's CAT fractal costs ~0.18 ms a redraw (~0.6 % of a core), no change in the speed with the CAT up (the same with `--audio-in` fed); 0.6.3's seven CAT fractals cost 0.32-0.60 ms a redraw in wasm3 (1.2-2.3 % of a core): 2.1x with the CAT off, 2.0-2.1x with it up on a loaded machine;
the v020 demo's project 5.9x.

## Trying it in the RemoteFM1 web player

The player at https://remotefm1.com supports Fantasy CFWs (RemoteFM1 `main`: `web/site/js/folder.js`).

1. Make a folder `RemoteFM1/` with an `audio/` folder inside, and copy some WAV or AIFF loops into it (for example
   `testloop_bpm120.wav` from FM1VST's `core-glue/elasticat/test_folder/audio/`).
2. Open https://remotefm1.com in Chrome or Edge (they can keep a picked folder; Safari and Firefox copy the files
   into the site's own storage instead).
3. Open the firmware browser, choose the **Open a file...** card, and pick `elasticat-0.6.1.rfm1` (or drop the file
   on the page). It goes into "Your firmware" with the Fantasy CFW badge.
4. When the page asks **Pick your RemoteFM1 folder**, pick the `RemoteFM1/` folder. On a later visit, click
   **Allow**. In Safari or Firefox, use **Add folder...** in the sheet's "RemoteFM1 folder" section.
5. Click the page to start the audio. When the screen shows the file, press PLAY (on the FM-1 or on the on-screen
   panel). Turn MASTER for the volume. The rest: [docs/FM1-CONTROLS.md](docs/FM1-CONTROLS.md).

Headless, end to end (a RemoteFM1 `main` export, `git -C <RemoteFM1> archive origin/main | tar -x -C <dir>`;
Chromium in `/Applications`): `node host/web_e2e.mjs --site=<dir> --rfm1=<the .rfm1>` opens the file, answers the
folder prompt with the demo loops, plays, programs steps, loads a file on T2, reloads; `results/web/`. On a site
with ain1 (RemoteFM1 `fw-catalog` c5866b9 on) it also feeds a fake microphone (Chromium's fake capture device playing
the demo's chord loop): T3 on INPUT is silent with NO INPUT while off, sounds once input is on, and a REC take lands
and is saved; on a site without ain1 those checks are skipped.

With RFM1Manager on this Mac (it serves its own copy of the site at `http://127.0.0.1:4717/rfm1/site/`): copy the
`.rfm1` into `RFM1Manager/out/` and open
`http://127.0.0.1:4717/rfm1/site/?rfm1=%2Ffiles%2Fout%2Felasticat-0.6.1.rfm1&rfm1id=elasticat`. That is what its
**Test in RemoteFM1** link does, but the link only lists firmwares in its catalog.

## Licence

GPL-3.0-only (LICENSE). Elasticat © Quixotic7.
