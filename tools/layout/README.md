# The layout tool

Elasticat's pages are data: which parameter sits on which knob, in which lane, on which page of which category; the
pages' names, order and pictures; each category's setup list; the bus pages; F3. The data is
[`design/layout/elasticat.json`](../../design/layout/elasticat.json). The build compiles it into
`src/app/ec_layout_gen.c` with `gen_layout.py`; a unit never loads a layout. The designer is a web page where you
rearrange that file and see the result on the unit's own screen.

Two things make the designer more than a drawing:

- **The preview is the firmware.** The screen in the page is Elasticat itself (every unit of `src/`) compiled to
  WebAssembly, drawing your design through its own `ec_pages.c` and screens, with the demo project playing (T1 drums
  with DESTROY and a send, T2 NEIGH, T3 HARMONIZER with an LFO, T4 R.POLY). What you see is what the unit shows.
- **Its output is the file the build reads.** Export gives `elasticat.json` in the generator's own canonical text.

## Open it

```sh
sh tools/layout/build_layout.sh            # build/layout-web (Emscripten from PATH, /opt/homebrew/bin/emcc or ~/emsdk)
python3 -m http.server 8765 --directory build/layout-web     # then http://127.0.0.1:8765/
```

In the Claude app: the launch configuration **`elasticat-layout`** (`.claude/launch.json`) serves the same folder at
http://127.0.0.1:8765/ in the browser pane (build it first). The design is kept in the browser as you work (local
storage); **Export** is what counts.

## The page

- **Left: the tree.** The nine categories in the panel's order (TRIG SRC FILT AMP FX MOD MST FILE PAT), each with its
  pages, each page with its lanes of four slots (KNOB 1-4). A slot shows the parameter's short name and its value on
  the selected track. The lane the unit shows now is marked orange. Below the categories: the buses (S1, S2, MST) and
  F3.
- **Centre: the preview** at 2x, with the live knob labels beside it (the unit's own `ec_app_labels`: what the
  host's panel shows), the category buttons (as on the unit: again = the next page), SELECT back / on (lanes, on into
  the next page), SETUP, HOME, PLAY. Above it the **context**: the target (T1-T6, S1, S2, MST), the track's machine,
  warp mode, filter machine, filter / amp env modes, insert FX and each bus's FX. Pages that depend on these show
  the variant for the context. Under it the checks (errors in red, warnings in yellow).
- **Right:** the selected slot (its parameter, a **label** of its own), the selected page's **note**, the **tray** of
  parameters of the category not on any of its pages in the current context (search finds any parameter, placed ones
  dimmed), and the **changes** from the shipped layout (what moved, page lists, notes).

Editing:

- **Drag** a parameter from the tray or from another slot onto any slot, on any page of any category: an occupied
  slot swaps. Drag a slot onto the tray to take it off. Click a tray chip to put it in the selected slot. Delete
  clears the selected slot.
- **Pages**: + PAGE, the grip (drag onto another page or category header), ↑ ↓, MOVE TO, ✕, the name (capitals; the
  header shows CAT/NAME), the picture (steps, waveform with its four captions, filter curve, filter / amp env, FX,
  sends, LFO 1 / 2, mod env, velocity, macros, macro matrix).
- **Lanes**: + LANE (up to 4 on a page, 2 in a setup list, 1 in F3), ↑ ↓ ✕. Empty slots are allowed.
- **Variants**: the VARIANT select shows which one the context picks (and picking one sets the context);
  + VARIANT FOR gives the current machine / warp / filter / env / FX its own lanes (a copy to edit); ✕ VARIANT.
- **Bus pages** use FX/FX's lanes for the bus's own FX machine; UNLINK gives a bus its own.
- **Knobs**: the wheel over a slot turns that knob on the unit (on its page and lane); the wheel or a drag on a knob
  beside the screen turns it. That changes the preview's sound only, never the design.
- UNDO / REDO (Ctrl+Z, Ctrl+Shift+Z), IMPORT (a file), OPEN (the shipped layout, or a design in `design/layout/designs/`),
  EXPORT.

What is **fixed** (the designer keeps it; the generator refuses otherwise): the categories themselves (the panel's
buttons); the five pages with behaviour of their own: MST/MIX (the mixer), MST/SCENE (the crossfader), FILE/SAMPLE
(the file list: SELECT is its cursor, SAVE loads), FILE/RAZOR (the slice editor: SELECT picks the slice, the OCT keys
split / delete) and PAT/PATTERN (the pattern grid): they stay in their category, once, with their picture; their
knob slots are yours (the RAZOR items `razor.*` work on the RAZOR page only). On a bus every category but MST, FILE
and PAT shows the bus's page, and the master bus's SRC is a mixer page. A page's caption over the step strip comes
from its category (TRIG: machine / length / rate, FILT: the filter machine, AMP: the env mode). The REC pop-up and
naming are not pages.

## Handing a design back

1. **Export** (a download, `elasticat.json`). Put it in the repo as `design/layout/elasticat.json` (the shipped
   layout) or as `design/layout/designs/<name>.json` (kept, openable from OPEN; not built).
2. `cmake --build build` runs the generator (it checks the file, writes `src/app/ec_layout_gen.c` and the page tables
   in `docs/FM1-CONTROLS.md`); `ctest --test-dir build` (the tests run on a fixed copy of 0.4.0's layout, so a new
   design never breaks them; `layout_check` and `layout_pack` check the new one).
3. The .rfm1: in the FM1VST worktree, `cmake --build build-rfm1 --target rfm1_elasticat` with `-DEC_SOURCE=<this
   repo>` (README.md, "The .rfm1"). The generated file is committed, so that build needs no Python.

`python3 tools/layout/gen_layout.py <file.json> --check` checks a design without writing anything.

## The design file

```json
{
  "format": "elasticat-layout", "version": 1, "name": "elasticat-0.4.0", "note": "",
  "categories": [                                  the nine, by name
    {"cat": "FILT",
     "pages": [
       {"title": "FILTER", "picture": "filter", "note": "free text",
        "lanes": [["filter_cutoff", "filter_res", "filter_type", "filter_env_depth"], ["filter_drive"]],
        "variants": [                              checked in order; the first that matches; else "lanes"
          {"when": {"filter": ["COMB"]},
           "lanes": [[{"item": "filter_cutoff", "label": "TUNE"}, "filter_res", null, "filter_env_depth"]]}]},
       ...],
     "setup": [["filter_machine", "filter_env_mode", "filter_mix_mode"]]},
    ...],
  "bus": [{"bus": "S1", "page": {"title": "SEND 1", "picture": "fx", "same_as": "FX/FX"}, "setup": [[...]]},
          {"bus": "S2", ...}, {"bus": "MST", ..., "src": {"title": "MIX", "picture": "mix", "lanes": [[...]]}}],
  "f3": [["pattern_steps", "pattern_rate", {"item": "global.glen", "label": "GLEN"}, {"item": "global.quant", "label": "QUANT"}]]
}
```

- A slot: `null` (an empty knob), a parameter's spec name (`src/seq/ec_params.c`: `filter_cutoff`), or
  `{"item": .., "label": ..}` for a label of its own. Named items that are not track parameters: `global.bpm`
  `global.swing` `global.glen` `global.quant` `global.rtn`, `slot.bpm` `slot.steps` `slot.gain` `slot.trim_start`
  `slot.trim_end`, `razor.start` .. `razor.sens` (RAZOR only), `e5.xfd` `e5.choke` `e5.gbpm` `e5.name` `e5.mute`.
- `when`: `machine` (`loop` = LOOP + TRIG, `slice` = SLICE + S.POLY, `razor` = RAZOR + R.POLY, `neighbor`, `input`),
  `warp` (TAPE VARI DOMINO GRAIN OLA PITCH HARM WTBL FREEZE FORMNT GSTR GSTR2 CHOP), `filter` (CLASSC MORPH LADDER
  COMB FORMNT SVF), `fenv` / `env` (ADSR AHR), `fx` (NONE DELAY REVERB .. LIMIT); several keys = all must hold. A
  variant may rename the page (`"title"`). Bus pages test their bus's own record (its FX machine).
- `picture`: steps, wave, wave-machine, wave-warp, wave-range, filter, fenv, aenv, fx, sends, lfo1, lfo2, menv, vel,
  macros, macro (+ `"macro": 1..4`); fixed: mix, scene, file, razor, pattern.

## The generator's checks

`gen_layout.py` stops (nothing written) on: an unknown item, picture, condition or field; an item twice on one page
variant (the same item on two pages is fine: 0.4.0 has FENV DEPTH on FILTER and F.ENV); a lane of more than four;
more than 4 lanes on a page (setup 2, F3 1); more than 16 pages in a category, or none; two pages of a category with
one name; a title wider than the header (CAT/TITLE, 16 characters in all) or in lower case; a label in lower case,
longer than 10 or with characters the screen's font lacks; a fixed picture outside its category or not there exactly
once; `razor.*` off the RAZOR page; a bad `same_as`; the buses not S1, S2, MST. It warns on: a parameter on no page
at all; a cell label longer than 5 (the panel's knob label gets cut); a variant that can never show.

## How it is built

- `gen_layout.py`: the checks, the C tables (`--c`), the packed text (`--pack`), the docs section (`--docs`), the
  canonical JSON (`--format`). The parameter ids come from `src/seq/ec_params.c` (spec names) and `ec_params.h`.
- `src/app/ec_layout.h`: the tables (pages of variants of items; masks over the option values), read only by
  `ec_pages.c`. The screens pick their picture by the page's (`EC_PIC_*`), not by the page's number.
- `layout_compile.js`: the same checks and the same pack in the page; `ec_layout_pack.c`: the pack read into a
  layout (the preview) and written from one; `ec_layout_wasm.c`: the preview's API (`lw_*`); `build_layout.sh`: the
  module (the demo folder and project embedded), the page.
- The packed form, one record a line, tab separated: `ECL1 name`; `C page0 npages setup` x 9; `B` the bus pages, bus
  setups, the master bus's SRC, F3; `P pic arg var0 nvar title`; `V mach warp filt fenv env fx item0 n title`;
  `I kind id flags label`.

## Tests

- `ctest`: `layout_040` (the tests' fixed layout gives exactly 0.4.0's hand-written pages: every page in every
  context hashed, `host/dump_pages.c`), `layout_check` (the shipped design passes), `layout_pack` (the compiled
  layout packed = the generator's pack; that pack loaded and packed again = the same; the loaded layout's pages = the
  compiled one's).
- `node tools/layout/test_designer.mjs` (after `build_layout.sh`; headless Chromium, no window): the page boots and
  plays; the page's pack of the shipped design = the generator's, and = what the firmware loaded; a parameter dropped
  onto another slot changes the preview's pixels, the knob and the live labels, and Export is exactly the expected
  JSON in the generator's canonical text; undo; a page moved to another category with a new lane; a bad title
  refused. `results/layout/` (screenshots, SUMMARY.txt).
