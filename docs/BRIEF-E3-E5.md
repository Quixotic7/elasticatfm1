# Shared brief: E3, E4, E5 in parallel (2026-11-02)

Three agents build E3, E4 and E5 of PLAN.md at the same time, each in its **own git worktree and branch** of this
repo; an integrator merges them afterwards. Read this, then your milestone's brief.

## Start
- Repo: `/Volumes/Q7Media-2025/Projects/Github/ChoralRootFM1/elasticatfm1` (main = 0.1.0, playable; tests green).
- Create your worktree from main: `git -C /Volumes/Q7Media-2025/Projects/Github/ChoralRootFM1/elasticatfm1 worktree add
  /Volumes/Q7Media-2025/Projects/Github/ChoralRootFM1/elasticatfm1-<eN> -b <eN> main` (eN = e3, e4, e5). Work only
  there. Never touch the main checkout, the other worktrees, or FM1VST (the integrator bumps the `.rfm1`).
- Read: PLAN.md, README.md, docs/FM1-CONTROLS.md, src/dsp/README.md + ec_dsp.h, src/seq/*.h, src/app/*.h,
  src/ui/ec_screens.h, design/mockup/*.html. The original (read-only spec):
  `/Volumes/Q7Media-2025/Projects/Github/ChoralRootFM1/Elasticat/` (README.md, docs/*, lib/**).

## Keep merges easy
- Put new work in **new files** (`src/dsp/ec_<thing>.c`, `src/seq/ec_<thing>.c`, `src/ui/ec_scr_<thing>.c`,
  `src/app/ec_<thing>.c`). Edits to shared files (ec_dsp.h, ec_internal.h, ec_dsp.c, ec_params.c, ec_project.*,
  ec_app.c, ec_pages.c, ec_screens.c, CMakeLists.txt, docs/FM1-CONTROLS.md) stay small: hooks, table rows, a
  dispatch case, appended sections marked `/* E3 */`, `/* E4 */`, `/* E5 */`.
- Project save format: add your fields as a **new tagged chunk** in the record (do not reorder existing ones), so
  the three versions merge and old saves still load. Bump nothing global; the integrator sets the version.
- Parameter tables: add rows; don't renumber existing params (append, or use the original's ids).

## Quality bar
- Port faithfully from the Lua + SuperCollider (+ `lib/ugens/src` C++); list deviations in src/dsp/README.md or
  docs/FM1-CONTROLS.md.
- Tests: `cmake -S . -B build && cmake --build build -j8 && ctest --test-dir build --output-on-failure` and
  `make -C tests/dsp` stay green; add focused tests for your features; `-Wall -Wextra` clean in clang **and** emcc
  (check with a quick emcc compile of src/ as the README's .rfm1 build does, or `emcc -c` each new file with the same
  flags).
- CPU: the whole cart must stay ≥ 2x real time in wasm3 (it is 9.5x now). Measure your heaviest case with
  `make -C tests/dsp bench` (native % of a core; wasm3 ≈ 22x native cost) and report it. Expensive things
  (FFT modes) are limited as the original limits them.
- Screens in the mockup's style (src/ui, 3x5 font at 2x, palette in PLAN.md); render PNGs of every new screen with
  `host/ec_run` into `results/<eN>/` (committed); WAVs stay untracked.
- FIZZ-style clarity: strong visuals, text only for names and values, nothing under ~12 px except tags.

## Rules
- `rm -rf` is blocked by a hook: `mv` to a trash folder. Write tool for long files. Never `pkill -f`. No foreground
  GUI windows. Don't push. Commit on your branch with messages ending
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report (concise)
What's built, controls added, deviations, tests + CPU numbers, PNG paths, files touched outside your new files (so the
integrator knows where conflicts can be), your branch's last commit.
