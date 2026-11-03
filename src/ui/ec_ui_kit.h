/* SPDX-License-Identifier: GPL-3.0-only */
/* The screen kit (docs/UX-V2.md §7, the approved mockups host/mock_ux2.c): the parts every mode screen is built from,
 * on the 120x120 grid in palette slots (ec_gfx.h). The firmware's screens (ec_screens.c ..) draw with these, and the
 * new mode screens do too. Geometry is the mockups': the header y 0..9, the picture y 10..75, the four cells y 76..119.
 *
 * Nothing here reads the app: callers pass the strings and numbers. */
#ifndef EC_UI_KIT_H
#define EC_UI_KIT_H
#include <stdint.h>

#include "ec_dsp.h"
#include "ec_gfx.h"

/* ------------------------------------------------------------------------------------------- header --- */
/* the header bar: the target box ("T1", "MST"), the title on the cream bar, the right-hand info (right-aligned
 * before the box), and the box at the right: arrows (a pop-up's back / OK, a wider box), else held > 0 (the count
 * of held steps on LOCK), else page ("1", "A"; NULL / "" none) */
void ec_kit_header(const char *target, const char *title, const char *page, const char *right, int held, int arrows);
/* a pop-up's two arrows (back, OK), 13 x 7 units from (x, y) */
void ec_kit_arrows(int x, int y, int c);

/* ------------------------------------------------------------------------------------------- cells --- */
enum {
    EC_KC_LOCK = 1u,      /* locked on the held step (or the held scene / pad): the whole cell in LOCK, drawn dark */
    EC_KC_ACC = 2u,       /* the value in the accent */
    EC_KC_DEPTH = 4u,     /* a bipolar bar from the centre: frac -1..1 */
    EC_KC_DIM = 8u,       /* the value dim (blocked: changes only while stopped) */
    EC_KC_SOME = 16u,     /* locked on some step of the pattern (no step held): a 2-unit LOCK outline round the cell */
    EC_KC_ACTUAL = 32u,   /* `actual` (0..1) is the resolved value: an accent mark on the bar */
    EC_KC_HOT = 64u,      /* a source key held, the cell is its depth: the label bright, the bar in SRC (blue) */
};
typedef struct {
    const char *label;    /* NULL / "": an empty cell (its K-tag dark) */
    const char *value;
    float frac;           /* the bar, 0..1 (EC_KC_DEPTH: -1..1) */
    unsigned flags;       /* EC_KC_* */
    const char *mark;     /* a matrix destination's source tag ("LFO1", "2 MOD") beside an accent dot, or NULL */
    float actual;         /* EC_KC_ACTUAL */
} ec_kit_cell_t;
/* the rules and the four cells (K1..K4) y 76..119; lanes > 1: the lane marks at y 72, the active one bright.
 * 0.6.1: no 1-unit detail: the bar is 2 units high at the box's y + 2 + EC_KIT_BAR_DY, on a lattice of
 * EC_KIT_BAR_CELLS 2-unit cells from the box's x + 3 */
#define EC_KIT_BAR_DY 17
#define EC_KIT_BAR_CELLS 27
void ec_kit_cells(const ec_kit_cell_t c[4], int lanes, int lane);
/* cell i's box (0..3: K1..K4), 23 rows: top cells y 76..98, bottom 97..119; a step lock fills it, EC_KC_SOME
 * outlines it (2 units) */
void ec_kit_cell_box(int i, int *x, int *y, int *w, int *h);
/* calls of ec_kit_cells so far (host/test_grid.c: the frames with param cells get the bar-area check) */
extern unsigned ec_kit_cells_drawn;

/* ------------------------------------------------------------------------------------------- the list --- */
typedef struct {
    const char *label, *value;
    uint8_t dim;          /* the value dim */
    const char *note;     /* after the label, in ALERT ("STOP"), or NULL */
    uint8_t two;          /* always two lines (THEME: the name on its own line) */
} ec_kit_row_t;
/* 0.6.1, the list menu (docs/UX-V2.md §10 "List menus"): every list (APP SETTINGS, the record settings, the modes'
 * settings, the recorder's setup) draws through this. One row is selected (an L3 band with a 2-unit accent edge);
 * SEL moves it, K1 changes its value (ec_app_list_knob). A row is EC_KIT_LIST_H1 high with the label left and the
 * value big at the right; when they don't fit on one line (or `two`) EC_KIT_LIST_H2: the label, then the value on
 * the second line. No knob tags. Rows between y0 and y1 (the full-screen lists: from y 12), EC_KIT_LIST_GAP apart
 * (none: only the selected row has a band, 2 units of it around its text); the window scrolls to keep `sel` (and
 * the row after it) in view, a 2-unit mark at the right edge shows where when not all fit. ys (n entries, or NULL):
 * each row's top y, -1 when not shown. */
#define EC_KIT_LIST_H1 14
#define EC_KIT_LIST_H2 24
#define EC_KIT_LIST_GAP 0
void ec_kit_list(const ec_kit_row_t *r, int n, int sel, int y0, int y1, int *ys);
int ec_kit_list_row_h(const ec_kit_row_t *r);

/* ------------------------------------------------------------------------------------------- the pop-up --- */
/* the mode-entry pop-up (docs/UX-V2.md §3) over the dimmed screen: the name big, the page in the accent, the 11 black
 * keys B1..B11 in their roles: 'P' page, 'S' source, 'X' special, 'A' action, 'O' unused; cur = the current page's
 * key (0..10) underlined, -1 none */
void ec_kit_popup(const char *name, const char *page, const char roles[11], int cur);
int ec_kit_role_colour(char role);

/* ------------------------------------------------------------------------------------------- pictures --- */
/* a waveform from an overview (n columns of peak 0..255) over grid columns x0..x1-1, centred in top..bot: columns in
 * [rs, re) in c_in, the rest c_out */
void ec_kit_wave(const uint8_t *ov, int n, int x0, int x1, int top, int bot, int c_in, int c_out, int rs, int re);
void ec_kit_playhead(int x, int top, int bot);      /* one accent column */

/* norns Elasticat's filter render (lib/ui/page_render.lua draw_filter_bars): 38 bars of 2 units + a 1-unit gap from
 * x 3 (norns: 42 on 128 px), growing up from bot. cut, res: the raw 0..127 values; type 1..4 LP HP BP NOTCH when
 * morph < 0, else the morph 0..1 (LP > notch > HP); depth -1..1 (the filter envelope): stopped, the band from the
 * cutoff toward where the envelope takes it is bright, else (or depth 0) the cutoff's bar alone */
#define EC_KIT_NBARS 38
void ec_kit_filter_bars(int top, int bot, float cut, float res, int type, float morph, float depth, int playing);

/* norns' env_bars / draw_envelope_bars: one amplitude per bar by bar counts (attack >= 1 bar, attack + decay <= half,
 * release <= half, sustain >= 1 bar, a 0 stage = 0 bars = a cliff). mode 1 ADSR, 2 AHR; raw values a d s hold r
 * (0..127, hold / release >= 128 = INF); hot: the stages (EC_KIT_ST_*) the active knobs edit, bright */
enum { EC_KIT_ST_A = 1, EC_KIT_ST_D = 2, EC_KIT_ST_S = 4, EC_KIT_ST_R = 8, EC_KIT_ST_H = 16 };
void ec_kit_env_bars(int top, int bot, int mode, float a, float d, float s, float hold, float r, unsigned hot);

/* the live grains (docs/UX-V2.md "Live grains"; the dsp's snapshot, ec_dsp_grains). Until src/dsp has it, the
 * struct is defined here */
#ifndef EC_GRAIN_VIEW_MAX
#define EC_KIT_LOCAL_GRAIN_VIEW 1
typedef struct {
    float pos, len, level;   /* pos / len: 0..1 of the sample; level: the envelope 0..1 */
    uint8_t newest;
} ec_grain_view_t;
#define EC_GRAIN_VIEW_MAX 16
#endif
/* each grain a block centred on its read position, as wide as its length, 3 units high, in a lane of its own (9
 * lanes 4 units apart from top: the scatter), shaded by level through the grey ramp, the newest in the accent;
 * x0..x1 = the sample's 0..1 (the mock's scr_loop) */
void ec_kit_grains(const ec_grain_view_t *g, int n, int x0, int x1, int top);
/* the grain view's frame (L8, one unit) round x, y, w, h and the emitter's accent notches at ex (< 0 none) */
void ec_kit_grain_frame(int x, int y, int w, int h, int ex);

/* a strip of 16 steps (x 3 + 7i, 5 wide) from top to bot: trigs filled (L12, the held one L15 and framed), empty
 * steps a dash, ghost trigs outlined; lock dots (LOCK) under the steps in locks; the playing step's accent bar over
 * it (play < 0 none). Bit i of each mask = step i */
void ec_kit_steps(int top, int bot, uint16_t trigs, uint16_t ghosts, uint16_t locks, uint16_t beyond, int held,
                  int play);
/* the 16 white keys as a ruler (x 3 + 7i): lit (L15, tall), in range (L8), else L3 / every 4th L5 */
void ec_kit_key_ruler(int y, uint16_t lit, uint16_t in_range, int n);
/* the nine track keys (T1..T6 S1 S2 MST, x 3 + 13i) at y: sel filled, muted dim */
void ec_kit_track_keys(int y, int sel, uint16_t muted);

/* ------------------------------------------------------------------------------------------- sprites --- */
/* norns Elasticat's sprites on the palette (tools/mockups/sprite_to_c.py --c -> src/ui/ec_spr_*.h): 120x64 frames,
 * 10 fps. The intro (q7logoanim, 55 frames, played once at power-on): the whole screen, the frame at y 28. The CAT
 * (dancingcat, 12 frames, looping at a speed that follows the tempo): y 10..119 on SHADE, the frame at y 22; the
 * caller draws the header and the track keys (ec_kit_track_keys(99, ..)) over it */
#define EC_KIT_SPR_W 120
#define EC_KIT_SPR_H 64
#define EC_KIT_SPR_FPS 10
int ec_scr_intro_frames(void);   /* 55 */
int ec_scr_cat_frames(void);     /* 12 */
void ec_scr_intro(int frame);    /* frame clamped */
void ec_scr_cat(int frame);      /* frame modulo the frames */
void ec_kit_sprite(const uint8_t *packed, int w, int h, int x, int y);   /* = ec_gfx_blit4, opaque */

#endif
