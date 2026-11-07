# QA 0.6.52: code, sanitizers, loader fuzz, cart build, docs (2026-11-06)

main at fc47945. Nothing under src/, host/, docs/, design/ changed; build/ untouched. probes/: small C programs
against build-qa-asan's libraries (host/test_ux.c's harness via #include). runs/: ec_run scripts and the 0.6.15
sweep. ../../../build-qa-asan: ASan + UBSan, Debug, -DCMAKE_DISABLE_FIND_PACKAGE_Python3=ON (gen_layout never
rewrites src/ or docs/). build-qa-warn: extra warnings, library only.

## Bugs

| # | Sev | What | How found | file:line |
|---|---|---|---|---|
| 1 | medium | A name with a leading "." (SHIFT+W1 first) is the hidden backup path of the same name without it: never listed, and the other project's save overwrites then deletes it. Any leading "." hides a project. | probes/probe_dotname.c (ZEBRA + ".ZEBRA"; save ZEBRA -> .ZEBRA.ecp gone) | src/app/ec_e5.c:313; ec_projfile.c:242, :95 |
| 2 | medium | F1+SEL dice roll INSERT 2's P1..P8 (and INS2 MIX) over the generic 0..128 integer range, not the mapped FX param's: RINGS MODE = 70, MOTION RATE = 58 stored. The engine clamps; cells, locks, scenes keep them. | probes/probe_dice_ins2.c | src/app/ec_app.c:963-971 vs :726 |
| 3 | low-med | A checksum-valid but unparseable record loads as an empty project ("NAME LOADED"); the next autosave overwrites the file; the hidden copy is never tried. | probes/probe_badparse.c (cut to 2/3, re-summed: load -1, "HALF LOADED", empty 6837 B file after one edit) | src/app/ec_projfile.c:671-672, :750 |
| 4 | low | The keybed offers "~"; the font has no glyph above 0x5F, it draws blank. | code | ec_e5.c:238; ec_gfx.c:112 |
| 5 | low | g_rsel (the LFO page route cursor) is global across tracks (clamped, harmless). | code | ec_modkeys.c:279 |
| 6 | low | Non-literal printf formats in say() and pages_set(): a label with "%" (the keybed types %) prints garbage. | -Wformat-nonliteral | ec_header.c:37-40, 131, 133; ec_mix.c:130 |
| 7 | low | memcmp on a struct that may hold padding: at worst an extra resend. | code | src/seq/ec_seq.c:816 |

## Sanitizer results

- ctest under build-qa-asan: 31/31 passed, no output. The 5 Python-gated tests by hand with the ASan binaries
  (probes/extra_tests.sh): layout_walk (4113 params, 48419 checks), layout_check, layout_pack, e0_run all pass.
- runs/s1.txt (editing, play, dice, LFO routes, save), runs/s2.txt (every PROJECTS action, dice on INSERT 2): exit 0.
- The 51 qa-0.6.15 scripts: 51 x exit 0, 0 sanitizer lines (runs/sweep_summary.txt). All probes clean.

## Warnings worth fixing

- Non-literal formats (ec_header.c, ec_mix.c); the wasm32 build also reports -Wformat-extra-args at ec_header.c:114-165
  and ec_mix.c:202.
- Unused rz (ec_razor_ui.c:351), unused button_text (ec_header.c:44), missing sens_ms initializer (ec_razor_ui.c:9).
- BK shadows a global (ec_scr_e5.c:136). `!best` tests a float in AUTO BPM (ec_file.c:835/838). An unannotated
  fall-through at ec_panel.c:817 (intended). 8 -Wcast-qual casts in ec_app.c.
- Of 192 extra-flag warnings, 118 -Wcomma and 28 -Wfloat-equal are style or intended (warnings.txt).

## Loader fuzz

- Seeds: s1's project (6.9 KB, v3), a rich v3 project (71 KB), the same as v1 (122 KB). Mutations: bit flips, edge
  bytes, truncation, NaN / inf / huge floats, block delete / insert, version byte, fake RZR1 / EC4B / ECRS tail
  chunks, random spans; three in four with length and checksum re-fixed so the parser really ran.
- Parse only: 2 x 20,000 records, ~3,950 loaded per run, no sanitizer reports; every loaded project round-trips,
  params in range, current / glen sane. Razor slices can load with start > end (repaired at use, ec_razor_range).
- Real open path (ec_pf_open, play, edit): 340 files, 271 loaded, 69 DAMAGED FILE, no errors ("loaded" includes the
  bug-3 case).
- By reading: every count guarded by the reader's bad flag, every index bounded, NaN handled, names sanitised, chunk
  lengths checked.

## Web / cart build

- No build_web.sh / test_web_emu.mjs in elasticatfm1 or the FM1VST worktree (they are ChoralRoot's); the web build
  here is the .rfm1 cart.
- build-rfm1-cat current (elasticat-0.6.52.rfm1, 741 KB; Homebrew emcc 6.0.13); ctest rfm1_elasticat|rfm1_skin 3/3.
- Every src unit compiles with ~/emsdk emcc 6.0.11 (wasm32, -Wall -Wextra -Wshadow -Wformat=2), 0 failures.

## Documentation mismatches

FM1-CONTROLS.md:
1. l.57: PRESETS turns the step page on every track page and PAGES, not only LOOP / TRIG (ec_panel.c:1018).
2. l.83 "ALGO picks the step page" is out of date.
3. l.1376 "ALGO = the step page" is out of date.
4. l.341 FILE -> PROJECTS -> SAMPLER; the cycle is SAMPLES -> PROJECTS -> EDITOR -> SLICE -> SAMPLER.
5. The F1 section (l.120-133) lacks 0.6.51's step held + F1.
6. The dice text (l.127) doesn't say they do nothing with a bus selected.

guide/index.html (0.6.48):
7. "B11 and the pages": B11 held as a step layer on FILTER / AMP / FX / MOD; since 0.6.51 LOOP / TRIG / KEYBOARD only.
8. The naming SHIFT set includes "~" (bug 4); a leading "." hides the project (bug 1).
9. 0.6.49-0.6.52 not covered.
10. The panel drawing's B2 reads SLICE; on a loop track it is LOOP.

Matching: the PROJECTS, SAMPLES, keybed, MIX and XFADE key maps; FM1-CONTROLS l.56; the dice's kept-params list.

## Checked and fine

- The 0.6.40-0.6.52 diffs: ec_lp_sel_mode gating, ec_mk_route*, ec_app_mod_values / ec_app_shown_values bounds, the
  dice state machine and its one undo (EC_U_NV 16), ec_mdep_nudge, the unipolar LFO math, the ms % 33 redraw (one tick
  per ms by the ABI).
- Name and path buffers bounded (EC_NAME, EC_PF_NAME, path[96], the list parser); no strcpy / strcat / sprintf in
  src.
- Mod matrix decode and clamps; all timers uint32 compared by difference; glyph lookup bounded; float == compares
  intended; ctest passes in parallel.

Disk: this folder ~1.1 GB (mostly runs/sweep/*/folder copies); build-qa-asan ~1.2 GB at the repo root (gitignored).
