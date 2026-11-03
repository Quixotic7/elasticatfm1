# Elasticat FM-1: mod matrix and MOD KEYS (spec, 2026-11-02, for 0.5.0)

The owner asked (2026-11-02):

- A **mod matrix page**, like ChoralRoot's. SELECT picks a row and the knobs edit that slot.
- **Modulation shortcuts on the black keys.** From any page, hold a source key and turn a parameter to set its depth.
- Sources: the LFOs, the mod envelope, the macros and scene A / B.
- **Hold MOD or SHIFT + MOD** turns the mode on. In it the black keys are the shortcuts, as in FIZZ.

Their notes in the elasticat-02 design say the same thing:
- The LFOs, the mod envelope, velocity and the macros all go through the matrix instead of fixed destinations.
- Add aftertouch, mod wheel and expression pedal.

The references are:
- ChoralRoot `firmware/src/cr_edit.c`: the STACK screen, quick modulation (`ce_map` / `ce_unmap`), the marks (`ce_marks`).
- ChoralRoot `firmware/src/eng_va.c:161-168`: the 8 × SRC / DST / AMT matrix.
- FIZZ `core/app/ctl_snd.c:566-626`: `snd_map` and `snd_modmap`.

## 1. The matrix

- **8 slots per track.** Each slot has **SRC**, **DST** and **DEPTH**.
- They are appended params: `mtx1_src`, `mtx1_dst`, `mtx1_depth` … `mtx8_*`. That adds 24 ids and keeps `EC_NP` under 255, because ids are u8 in saves and locks. The enum is only appended to.
- **SRC**: OFF, LFO 1, LFO 2, ENV (the mod envelope), VEL, AT, MW, EXPR, M1, M2, M3, M4.
  - AT is aftertouch: the core's `key_lean`, the highest of the track's held keys, 0..1.
  - MW and EXPR are MIDI CC 1 and CC 11 from `midi_in`, on any channel.
  - The LFOs are bipolar. The other sources are unipolar. Each LFO keeps its own DEPTH param as the source's level.
- **DST**: every continuous (morphable, `EC_PK_NUM`) track param, plus PITCH, which is the existing fast pitch bus.
  - CUT, RES, AMP and PAN go through the existing fast buses. Every other destination gets a generic offset (below).
  - The list is grouped by category in category order.
  - **SHIFT + the DST knob** jumps a whole category. A plain turn steps one item, with no acceleration.
- **DEPTH**: -100..+100 % of the destination's range, bipolar.
  - Steps are fine near 0, like ChoralRoot's AMT. SHIFT snaps to 0, ±25, ±50, ±100.
  - DEPTH can be locked to a step and morphed by scenes. SRC and DST can't.
- **Generic destinations.** Once per chunk, the value goes through the usual resolve (step lock > scene > track), then has Σ source × depth × range added and is clamped.
  - This is computed only for params that some slot points at.
  - It reaches the DSP the same way a lock does, so it is smooth per chunk.
- **The macros** become sources and lose their fixed per-destination depths: the 20 `macroN_*_depth` params retire.
  - The MACRO n page keeps VAL. Its picture lists that macro's matrix rows, read-only.
- **Old projects (migration).** A project with no `mtx*` params is converted on load into slots, filled in this order:
  1. LFO 1's dest
  2. LFO 2's dest
  3. ENV's dest
  4. VEL's dest
  5. each non-zero macro depth

  The old depth values convert at the same strength. If a source aimed at a macro (M1..M4), the slot's DST is that macro's VAL. Anything past 8 slots is dropped, and the test reports it. The old dest and depth params stay in the enum (never reused) and leave every page.

## 2. The MATRIX page

- **MOD / MATRIX** is the first MOD page. It is a special page with its own picture, like FILE / RAZOR.
  - The designer keeps it in its category. Its knob slots are fixed and fully defined.
- The picture shows **8 compact rows** of SRC · DST · DEPTH. Each DEPTH has a centre bar.
  - The active row is bright cream. The other rows are grey. A source name is shown in its source colour only where it already has one.
- **SELECT** picks the row. Past row 8 it runs on to the next page, like `lane_step`.
- **KNOB 1** = SRC, **KNOB 2** = DST, **KNOB 3** = DEPTH, **KNOB 4** = nothing.
- **Marks.** Any cell on any page whose param is a destination of a slot on this track gets a small mark: a dot in the cream/orange look, or the source tag where there is room.
- The LFO 1, LFO 2, ENV and VELOCITY pages lose their DEST, which is already the case in the owner's layout.

## 3. MOD KEYS (black keys = sources)

- **Hold MOD (400 ms) or SHIFT + MOD** turns MOD KEYS on, and doing it again turns it off. HOME also turns it off.
  - The mode stays on after you let go; the owner prefers single clicks.
  - While it is on, the MOD button stays lit.
  - MOD has no setup, so both gestures are free.
- In MOD KEYS the 11 black keys (the upper row; today T1-T6 S1 S2 MST FILL PATN) are:

  | B1 | B2 | B3 | B4 | B5 | B6 | B7 | B8 | B9 | B10 | B11 |
  |---|---|---|---|---|---|---|---|---|---|---|
  | LFO 1 | LFO 2 | ENV | VEL | AT | M1 | M2 | M3 | M4 | SCN A | SCN B |

  MW and EXPR are set from the MATRIX page only. ALGORITHM still picks the track.
- The labels come from `ec_app_labels`. All 11 keys are lit dim, and a held key is bright.
- **Hold a source key and turn any value knob** (KNOB 1-4 on whatever page is showing):
  - The first detent finds the slot with that SRC → that param. Otherwise it takes the first free slot (SRC = OFF) and sets DST.
  - Later detents set its DEPTH.
  - If the param can't be a destination, show a "NO MOD" flash. If all 8 slots are taken, show a "FULL" flash. Both reuse the takeover overlay.
  - While the key is held, the turned cell shows the source name and the DEPTH as a centre bar. If the param isn't on the screen, the takeover overlay shows it.
  - **OCT+ held as well**, then turn: clear that source's slot for that param. This is Elasticat's existing "OCT+ clears".
  - When the key is let go, a slot this hold created and left at DEPTH 0 is freed.
  - **With steps held**, the DEPTH becomes a lock on those steps, as in FIZZ.
- **A tap with no turn** opens that source's page:
  - LFO 1 / LFO 2 / ENV / VELOCITY / MACRO n go to their own page.
  - AT goes to MATRIX.
  - SCN A / SCN B go to MST / SCENE.
- **SCN A / SCN B held + a knob** writes that scene's value for the param. This is the existing scene anchor path (`ec_e5_knob_cell`), not the matrix.
- The usual SHIFT layer on the black keys is unchanged outside MOD KEYS.

## 4. Must hold

- Tests: ctest and the DSP suites green; the designer test and the web e2e green.
- New tests:
  - the matrix resolve (generic and fast-bus destinations)
  - migration of a 0.4 project, with LFO → CUT still audible the same
  - each MOD KEYS gesture through the `ec_run` harness
  - `gen_layout --check` with no "not on any page" warning
- Worst case is still ≥ 2× real time in wasm3. Report the number.
- Version **0.5.0**: core_elasticat.cmake expects ec0.5.0. Update the docs: FM1-CONTROLS.md, PLAN.md, descriptions.json for the new page and params.

## Built in 0.5.0

What the spec left open, and how it was decided (the simplest thing consistent with ChoralRoot / FIZZ):

- **Ids.** `mtx1_src` .. `mtx8_depth` are ids 202..225 (`EC_P_MTX1_SRC` ..); `EC_NP` = 226, asserted <= 255.
  The retired params carry the flag `EC_PF_RETIRED` (the old bit `EC_PF_E4`): lfo1/lfo2/menv/mvel `_dest` and the
  20 macro depths. Nothing reads them; they stay in the enum for old saves.
- **Storage.** SRC is an option 1..12 (1 = OFF). DST is the destination's param id + 1 (0 = OFF), so a save never
  depends on the layout. DEPTH is -100..100 in %.
- **What can be a destination** (`ec_param_mod_dest`): a continuous param that is not sequencer-only, not retired,
  not a matrix DEPTH and not structural (the scenes' MORPH_EXCLUDE list: steps, slot, slice count / index, chop
  steps, loop division, rate). VOL is one (it rides the AMP bus) although the scenes leave it alone.
- **The scale.** A generic destination moves by source x DEPTH x (max - min). The fast buses keep their 0.4 swing
  at 100 %: PITCH +-12 st, CUT +-120 st (the whole range), RES +-0.5, AMP x(1 +- 1), PAN the whole width. So the
  migration is exact: a 0.4 LFO / ENV / macro on CUT (scaled x0.3 there) becomes DEPTH 30 %, VEL on CUT 100 %,
  everything else 100 % (an LFO / ENV / VEL keeps its own DEPTH as the level), a macro depth d becomes d x 100 %
  (x 30 % on CUT).
- **Where the generic offset is computed.** The sequencer's flush (`ec_seq.c`): it resolves every param (lock >
  scene > track), and for a track with a live generic slot reads the engine's sources (`ec_dsp_mod_sources`, one
  block old) and adds the offsets (`ec_mtx_apply`), then sends the params as for a lock. Such a track is re-sent
  every audio block. M1..M4 as sources are the resolved VALs plus any slot aimed at them (the slots aimed at a
  macro VAL are applied first).
- **The engine** (`ec_mod.c`) computes the sources each chunk (LFO / ENV / VEL at their levels, AT / MW / EXPR
  smoothed, M1..M4 from the VALs it is sent) and sums the slots aimed at its five buses; it runs while any slot is
  live, whatever its destination.
- **AT** is the highest `key_lean` among the held keys, sent to the selected track only (the keys play it); the
  others get 0. **MW / EXPR** are the last CC 1 / CC 11 on any channel, on every track.
- **A source's level at 0.** Making a slot (MOD KEYS, or SRC / DST on the MATRIX page) for LFO 1, LFO 2, ENV or VEL
  whose own DEPTH is 0 sets that DEPTH to full (+64): otherwise a new route would be silent.
- **The first detent.** A new route: the first detent only makes the slot (DEPTH 0), as the spec says. A route that
  already exists: every detent, the first included, moves its DEPTH (there is nothing to find).
- **Freed on release**: a slot this hold made, left at DEPTH 0 and with no lock on its DEPTH (a held-steps hold
  leaves the base at 0 and the locks hold the depth: it stays).
- **The held source's cells.** Every cell whose param has a slot from the held source shows SOURCE>PARAM and the
  DEPTH (a held step's lock in orange) with a centre bar, not only the cell just turned.
- **The marks**: an orange dot and the source's tag in the cell's top right (in place of the K1..K4 tag); several
  sources: "2 MOD"; no room beside a long label: the dot alone.
- **NO MOD / FULL** show in the takeover (the key's name over the word).
- **MOD hold** no longer opens the MOD pages (it used to be a tap): it only toggles MOD KEYS. SHIFT + MOD likewise.
  "MOD KEYS" / "MOD KEYS OFF" flash in the header.
- **SHIFT inside MOD KEYS**: SHIFT's black-key layer (memorize, recall, copy / paste pattern, tap, latch, name)
  still works; a black key pressed with SHIFT is never a source.
- **Scenes in old projects**: a scene value of an old macro depth moves onto its slot's DEPTH (current pattern's
  tracks); one whose route was dropped, or whose track value was 0, is dropped.
- **SELECT onto the MATRIX page backwards** lands on row 8 (as `lane_step` lands on a page's last lane).
- **The layout**: `mtx.src` / `mtx.dst` / `mtx.depth` items (`EC_IT_MTX`) on the fixed picture `matrix`
  (`EC_PIC_MATRIX`); a design without it is a warning (MOD KEYS still reaches the matrix); the mtx params are not
  placeable, the retired ones are a warning when placed. The designer adds the MATRIX page to an imported design
  that lacks it.
- **Tests**: ctest `mtx` (host/test_mtx.c, 68 checks: the resolve, AT / CC 1 / CC 11 through the engine, a 0.4
  record converted (LFO1 -> CUT = 0.3 x the LFO on the bus, 10 routes -> 2 dropped, scene values moved), the MATRIX
  page, every MOD KEYS gesture). `ec_make_heavy` now fills every slot of every track (generic destinations
  included) for the wasm3 CPU check.
