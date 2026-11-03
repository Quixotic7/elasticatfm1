# 0.6.0 mockups

One picture set per mode of docs/UX-V2.md, for the owner's review. Mockup tooling only: nothing here changes the
firmware.

```sh
sh tools/mockups/make_mockups.sh            # build/ec_mock_ux2 -> results/ux-v2-mockups/ (not committed)
```

The norns sprites are read from `../Elasticat/images/` (or `ELASTICAT_NORNS=<path>`), never copied into this repo;
`sprite_to_c.py --c out.h` turns a sheet into the C array (4-bit palette indices) the firmware build can include.

It needs a configured `build/` (`cmake -S . -B build`), node, python3 and Chromium at
`/Applications/Chromium.app` (or `CHROMIUM=...`); Chromium runs headless with a fresh profile, never a window.

| File | What | Made by |
|---|---|---|
| `<Mode>_screen.png` | the mode's main screen, 480x480 | `host/mock_ux2.c` |
| `<Mode>_popup.png` | the mode-entry pop-up over the dimmed screen: the name, the page, the 11 black keys in their roles | `host/mock_ux2.c` |
| `<Mode>_panel.png` | the FM-1 as RemoteFM1 draws it (desktop layout), with the mode's live labels, lit button and key LEDs, and its screen in the LCD, 2184x1418 | `render_panels.mjs` + `panel.html` |
| `AppSettings_*` | SHIFT + HOME's APP SETTINGS (THEME active) | both |
| `Filter_env_screen.png`, `Mod_env_screen.png` | the FILTER ENV and MOD ENV pages (FILTER, FILTER ENV, AMP ENV, MOD ENV use norns Elasticat's bar renders: 38 bars on the 120 grid where norns has 42 on 128) | `host/mock_ux2.c` |
| `Intro_screen.png`, `Intro_frames.png`, `Intro.gif` | norns Elasticat's launch logo (55 frames, 10 fps), on the palette | `sprite_to_c.py` + `host/mock_ux2.c` |
| `Cat_screen.png`, `Cat_frames.png`, `Cat.gif` | MIX's B11 CAT (norns' Visualizer sprite, 12 frames) | `sprite_to_c.py` + `host/mock_ux2.c` |
| `_palette.png` | the 16 palette slots of the default theme | `host/mock_ux2.c` |
| `_themes.png` | the MATRIX screen in all six themes, with their swatches | `host/mock_ux2.c` |
| `palette.json` | the theme tables, for the check | `host/mock_ux2.c` |

**The rules** (owner, 2026-11-03), enforced by `check_png.py` (the run fails on any break):
- Everything is designed at **120x120** and scaled by exactly 2 to the 240x240 LCD: no 1-pixel details. The screens are
  drawn as palette indices into a 120x120 buffer, in the mockup grid's units (the units of ec_screens.c's `R()` / `T()`),
  with the firmware's own 3x5 font (`ec_gfx_glyph`) and the real header / cell geometry. Every 4x4 block of a 480 PNG is
  one colour. In a panel PNG only its LCD is checked (minus the bezel's rounded corners): the rest is RemoteFM1's.
- **16 colours**: one table per theme (`host/mock_palette.h`), slots BG BLACK L3 L5 L8 L12 L15 ACC (today's `EC_C_*`)
  plus SRC SPECIAL ACTION LOCK ALERT ACC_DIM SHADE OK. A theme is a table swap. Each PNG uses only its theme's colours.

`python3 tools/mockups/check_png.py --survey results` reports which of the firmware's own screenshots break the grid rule.

Where the content lives: the screens and the pop-ups' key roles in `host/mock_ux2.c` (`MODES[]`); the panel labels, lit
buttons and key roles in `modes.js` (keep the two in step). The panel code is the vendored `tools/layout/fm1/panel.js`,
unchanged.
