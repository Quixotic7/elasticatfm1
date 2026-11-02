# Elasticat FM-1

Elasticat, the norns sample-mangling sequencer ([Quixotic7/Elasticat](https://github.com/Quixotic7/Elasticat),
GPL-3.0), rewritten in C for the M-VAVE FM-1 as a **Fantasy CFW**: a host-only `.rfm1` that runs in RemoteFM1 hosts
(the remotefm1.com web player first), with the FM-1 as its panel and samples from the shared RemoteFM1 folder
(`RemoteFM1/audio/`). The plan, panel mapping and milestones are in [PLAN.md](PLAN.md).

**Status: E0, "the pipe".** At power-on the cart lists `/audio` (recursively), loads the first WAV / AIFF it finds,
and PLAY loops it at its own speed. MASTER sets the volume. SELECT moves the file cursor and FILE (the SAVE button)
loads the file under it. The lower keys chase the playhead in 16 steps, and the screen echoes every key, button and
knob. With no folder (or no audio files) a built-in synthesized loop plays.

## Layout

```
src/plat/   fm1core ABI glue (ec_core.c: fm1core_get, the panel, LEDs, LCD, the 1 MiB flash image with its versioned
            header), ec_plat.h (what the rest of the code sees), ec_e0.c (the E0 app, temporary), abi/ (copies of
            FM1VST's fm1core.h and rfm1_host.h for the native build)
src/io/     ec_fs (async file API over rfm1_host_1), ec_fs_native (a native stand-in serving a real folder, one tick
            late), ec_browse (recursive /audio scan), ec_audiofile (WAV / AIFF / AIFC parse + decode), ec_loader
            (chunked loading), ec_samplestore (int16 stereo arena + handles, bpm from file names)
src/ui/     ec_gfx (RGB565, palette, rects / lines / dots, the mockup's 3x5 pixel font), ec_leds, ec_screen_e0
src/dsp/    the sound engine (written separately; ec_dsp.h)
host/       ec_run (headless native harness: scripted input, WAV + PNG out), test_io (the file layer's test),
            make_testloop.py (the 4 s test loop)
results/e0/ E0's proof files
```

## The APIs, briefly

- **`ec_plat.h`:** buttons by label (`EC_B_PLAY`, `EC_B_SAVE`, ..; Elasticat's names in the comments), knobs by role
  (`EC_E_SELECT`, `EC_E_K1`..), the keys as rows (`ec_key_lower/upper`, `ec_key_of_lower/upper`). One `ec_input_t`
  per device ms (held / down / up masks, velocities, detents by role, MASTER 0..1023). LEDs: `ec_plat_led_lower /
  upper / button / play_green`. The LCD: `ec_plat_lcd()` (big-endian RGB565) + `ec_plat_lcd_commit()`. The flash
  record: `ec_plat_record()`. The app implements `ec_app_init / tick / frame / audio / ui_info / shutdown`.
- **io:** `ec_fs_start / ec_fs_update / ec_fs_cancel` on a caller-owned `ec_fsreq_t` (never blocks, at most 32 in
  flight, at most 1 MiB per request). `ec_browse_start("/audio")` + `ec_browse_step()` per tick. `ec_load_start(path)`
  + `ec_load_step()` per tick: STAT, header reads, then data in ≤ 1 MiB reads, decoded 4096 frames per tick into the
  store. `ec_ss_alloc / get / ready / reset`: int16 stereo (mono doubled) in a 96 MiB static arena, with rate,
  channels, bpm (`ec_bpm_from_name`, Elasticat's rule) and a 128-column overview.
- **ui:** `ec_gfx_begin(lcd)`, `ec_gfx_rect / line / dots_h / text_al` in physical pixels; the mockup's 120x120 grid
  is scale `EC_U` = 2. Palette `EC_C_BG, L3, L5, L8, L12, L15, ACC`. `ec_leds_steps(trigs, playhead)`,
  `ec_leds_upper(sel, active)`.

## Building and testing natively

```sh
cmake -S . -B build && cmake --build build -j8 && ctest --test-dir build --output-on-failure
build/ec_run --folder <RemoteFM1 root> --press PLAY@1000 --wav out.wav --png screen.png --png-scale 2
```

`ec_run` options are listed at the top of `host/ec_run.c`: `--press PLAY@MS`, `--turn SELECT@MS:+1`, `--key 9@MS`,
`--master 512@MS`, `--png-at MS:file`, `--info-at MS`, `--expect-ui`, `--min-rms`.

## The .rfm1

It is built by FM1VST from this repo (submodule `cores/elasticatfm1`, glue `core-glue/elasticat`, branch
`rfm1-elasticat`). See that glue's README.md. In short, in the FM1VST worktree:

```sh
cmake -S . -B build-rfm1 -DFM1_RFM1=ON -DFM1_RFM1_ONLY=ON -DFM1_ONLY_CORES=elasticat -DFM1_PUBLIC_BUILD=ON \
      -DFM1_EMCC=/opt/homebrew/bin/emcc -DCR_PYTHON=<RFM1Manager>/.venv/bin/python [-DEC_SOURCE=<this repo>]
cmake --build build-rfm1 -j8 --target rfm1_elasticat rfm1_run
ctest --test-dir build-rfm1 -R rfm1_elasticat -V        # rfm1_check + rfm1_run --folder core-glue/elasticat/test_folder
```

## Trying it in the RemoteFM1 web player

The player at https://remotefm1.com supports Fantasy CFWs (RemoteFM1 `main`: `web/site/js/folder.js`).

1. Make a folder `RemoteFM1/` with an `audio/` folder inside, and copy some WAV or AIFF loops into it (for example
   `testloop_bpm120.wav` from FM1VST's `core-glue/elasticat/test_folder/audio/`).
2. Open https://remotefm1.com in Chrome or Edge (they can keep a picked folder; Safari and Firefox copy the files
   into the site's own storage instead).
3. Open the firmware browser, choose the **Open a file...** card, and pick `elasticat-0.0.1.rfm1` (or drop the file
   on the page). It goes into "Your firmware" with the Fantasy CFW badge.
4. When the page asks **Pick your RemoteFM1 folder**, pick the `RemoteFM1/` folder. On a later visit, click
   **Allow**. In Safari or Firefox, use **Add folder...** in the sheet's "RemoteFM1 folder" section.
5. Click the page to start the audio. When the screen shows the file, press PLAY (on the FM-1 or on the on-screen
   panel). Turn MASTER for the volume. Turn SELECT, then press SAVE (FILE) to load another file.

With RFM1Manager on this Mac (it serves its own copy of the site at `http://127.0.0.1:4717/rfm1/site/`): copy the
`.rfm1` into `RFM1Manager/out/` and open
`http://127.0.0.1:4717/rfm1/site/?rfm1=%2Ffiles%2Fout%2Felasticat-0.0.1.rfm1&rfm1id=elasticat`. That is what its
**Test in RemoteFM1** link does, but the link only lists firmwares in its catalog.

## Licence

GPL-3.0-only (LICENSE). Elasticat © Quixotic7.
