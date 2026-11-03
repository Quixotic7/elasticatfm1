/* SPDX-License-Identifier: GPL-3.0-only */
/* test_grid: the owner's graphics rules (docs/UX-V2.md §7) on the firmware's own screens. With the demo folder
 * (host/make_demo_folder.py) booted through the core, a broad set of screens is reached by setting the app state
 * directly (category, page, settings, the pop-ups, F3, naming, the scene page, REC, the takeover, SHIFT's map, a question,
 * held steps with locks, the machines and warps), not by gestures; plus the kit's parts (ec_ui_kit.h) drawn alone:
 * the cells, the norns filter / envelope bars, the grains, the settings list, the intro and the CAT frames. For every LCD frame,
 * in each of the six themes: every 2x2 block is one colour, and every colour is one of the theme's 16.
 * 0.6.1 (docs/UX-V2.md §10): SRC is each theme's blue, clear of ACC / LOCK / ALERT; the frames that showed the
 * owner's 1-unit depth bar (every page with its params modulated, a held source key, held / outlined / no step locks,
 * the cells alone with tiny depths). 0.6.1 wave 2: on every frame (but MIX / CAT for now), the fine check (fine_runs:
 * no line, bar, marker or rule 1 unit thick, text and sprites exempt by ec_gfx's text mask).
 *   ec_test_grid <folder> [png dir]     (png dir: the CREAM frames, for a look; results are not committed)
 *   EC_GRID_DUMP=1 ec_test_grid ..      every bad run of every CREAM frame; with the library built with -DEC_GFX_WHO
 *                                       also the address of the drawing call (atos / addr2line it) */
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_fs_native.h"
#include "ec_gfx.h"
#include "ec_mtx.h"
#include "ec_holdmenu.h"
#include "ec_png.h"
#include "ec_screens.h"
#include "ec_ui_kit.h"
#include "fm1core.h"

static int fails, checks, shots, fine_frames, fine_cells;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        checks++;                                                                                                  \
        if (!(c)) {                                                                                                \
            fails++;                                                                                               \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                            \
            printf(__VA_ARGS__);                                                                                   \
            printf("\n");                                                                                          \
        }                                                                                                          \
    } while (0)

#define A (&ec_app)
#define LW EC_GFX_LCD_W
#define LH EC_GFX_LCD_H
static const fm1core_t *C;
static fm1panel_t *P;
static uint32_t ms;
static int g_theme;
static const char *g_png;
static uint16_t g_kitlcd[LW * LH];

static void run(int n)
{
    int i;
    for (i = 0; i < n; i++) {
        ec_fs_native_pump();
        C->tick(ms++);
        if (ms % 15 == 0)
            C->frame();
    }
}

/* 0.6.1 (docs/UX-V2.md §7 / §10, the owner's "1 pixel" = one unit of the 120 grid): the fine check. The 2x2 rule
 * below only says the frame is on the 120 grid: a bar 1 grid unit high is 2x2 blocks and passes it. So, on the canvas
 * as drawn (the palette indices, every unit of every frame): no run of one colour other than the ground (BG) exactly
 * 1 unit thick (both neighbours across it another colour, or the screen's edge) and at least 3 units long, across or
 * down: no line, bar, marker, tick, outline or rule thinner than 2 units. Text is exempt: ec_gfx marks the units the
 * last text call drew (ec_gfx_text_mask, test builds), and a text unit counts as the run's own colour across it (so a
 * 1-unit gap between a glyph and a bar in a filled cell is not a run either). Returns the bad runs; *where: the
 * first. */
static int g_who_dump;
/* 0.6.1 wave 2: MIX and the CAT are being redrawn by v61b-gfx (MIX's pan, the CAT's stage); their frames skip the fine
 * check until that merges (the 2x2 and palette checks still run). Counted in fine_skipped. */
static int g_skip_fine, fine_skipped;
#ifdef EC_GFX_WHO
void *const *ec_gfx_who(void);
#endif
static int fine_runs(char *where, size_t cap)
{
    const uint8_t *cv = ec_gfx_canvas(), *tx = ec_gfx_text_mask();
    int pass, line, at, bad = 0;
    for (pass = 0; pass < 2; pass++)                       /* 0: runs across (rows), 1: runs down (columns) */
        for (line = 0; line < EC_GW; line++)
            for (at = 0; at < EC_GW;) {
                const int d = pass ? EC_GW : 1, e = pass ? 1 : EC_GW;  /* along, across */
                const int i0 = pass ? at * EC_GW + line : line * EC_GW + at, c = cv[i0];
                int n = 0;
                for (; at + n < EC_GW; n++) {
                    const int i = i0 + n * d;
                    if (tx[i] || cv[i] != c || c == EC_C_BG)
                        break;
                    if ((line > 0 && (cv[i - e] == c || tx[i - e])) || (line < EC_GW - 1 && (cv[i + e] == c || tx[i + e])))
                        break;
                }
                if (n >= 3) {
                    const int x = pass ? line : at, y = pass ? at : line;
                    if (!bad++)
                        snprintf(where, cap, "a 1-unit run %s at (%d, %d), %d long", pass ? "down" : "across", x, y, n);
                    if (g_who_dump && g_theme == EC_TH_CREAM) {
                        printf("  RUN %c %d %d %d c%d", pass ? '|' : '-', x, y, n, c);
#ifdef EC_GFX_WHO
                        printf(" @%p", ec_gfx_who()[i0]);
#endif
                        printf("\n");
                    }
                }
                at += n ? n : 1;
            }
    return bad;
}

/* the rules on one LCD frame (cells: the frame drew param cells, so the bar-area check too) */
static void check_lcd_cells(const uint16_t *lcd, const char *name, int cells)
{
    uint16_t pal[EC_NPAL];
    int x, y, i, bad = 0, off = 0, used = 0;
    uint16_t seen[EC_NPAL];
    for (i = 0; i < EC_NPAL; i++)
        pal[i] = ec_gfx_lcd_colour(g_theme, i);
    for (y = 0; y < LH; y += 2)
        for (x = 0; x < LW; x += 2) {
            const uint16_t c = lcd[y * LW + x];
            bad += lcd[y * LW + x + 1] != c || lcd[(y + 1) * LW + x] != c || lcd[(y + 1) * LW + x + 1] != c;
        }
    for (i = 0; i < LW * LH; i++) {
        int k, in = 0;
        for (k = 0; k < EC_NPAL && !in; k++)
            in = lcd[i] == pal[k];
        off += !in;
        for (k = 0; k < used && seen[k] != lcd[i]; k++)
            ;
        if (k == used && used < EC_NPAL)
            seen[used++] = lcd[i];
    }
    CHECK(!bad, "%s (%s): %d 2x2 blocks of more than one colour", name, ec_gfx_theme_name(g_theme), bad);
    CHECK(!off, "%s (%s): %d pixels outside the theme's 16 colours", name, ec_gfx_theme_name(g_theme), off);
    CHECK(used >= 2 || !strcmp(name, "kit_intro"), "%s (%s): a blank screen", name, ec_gfx_theme_name(g_theme));
    if (!g_skip_fine) {
        char where[80] = "";
        int runs;
        if (g_who_dump && g_theme == EC_TH_CREAM)
            printf("FRAME %s\n", name);
        runs = fine_runs(where, sizeof where);
        CHECK(!runs, "%s (%s): %d 1-unit runs (%s)", name, ec_gfx_theme_name(g_theme), runs, where);
        fine_frames++;
        fine_cells += cells;
    } else
        fine_skipped++;
    if (g_png && g_theme == EC_TH_CREAM) {
        char path[512];
        snprintf(path, sizeof path, "%s/%03d_%s.png", g_png, shots, name);
        ec_png_write_lcd(path, lcd, LW, LH, 2);
    }
    shots++;
}

/* the app's own frame of the state set up */
static void shot(const char *name)
{
    const unsigned n0 = ec_kit_cells_drawn;
    A->redraw = 1;
    g_skip_fine = 0;
    C->frame();
    check_lcd_cells(P->lcd, name, ec_kit_cells_drawn != n0);
}

static void k_cat(int f);
/* the kit alone: draw(), presented to a buffer of our own */
static void kit_shot(const char *name, void (*draw)(int), int arg)
{
    const unsigned n0 = ec_kit_cells_drawn;
    g_skip_fine = 0;
    ec_gfx_begin(g_kitlcd);
    ec_gfx_fill(EC_C_BG);
    draw(arg);
    ec_gfx_present();
    ec_gfx_begin(ec_plat_lcd());
    check_lcd_cells(g_kitlcd, name, ec_kit_cells_drawn != n0);
}

static void reset_state(void)
{
    A->settings = 0;
    A->bus = 0;
    A->track = 0;
    A->step_held = 0;
    A->ux.f3 = 0;
    A->e5.naming = 0;
    A->e5.confirm = 0;
    A->rec.open = 0;
    A->ux.to_ms = 0;
    A->fn = 0;
    A->msg_ms = 0;
    A->scr = 0;                                    /* 0.6.0: the modes */
    A->pn.intro = 0;
    A->pn.cat = 0;
    A->pn.as_open = 0;
    A->pn.shift_held = A->pn.shift_sticky = 0;
    A->pn.target = 0;
    A->pn.rs_open = 0;                             /* 0.6.0 part 2 */
    A->msg_alert = 0;
}

/* ------------------------------------------------------------------------------------------- the kit --- */
/* 0.6.1: the cells alone in every state: values and depths from tiny to full (the owner's 1-unit bar), the step
 * lock fill, the outline, a held source (SRC), the actual mark, dim */
static void k_cells(int i)
{
    static const float D[10] = {0.001f, -0.001f, 0.03f, -0.04f, 0.5f, -1.0f, 1.0f, 0.96f, -0.96f, 0.08f};
    static const float F[6] = {0.001f, 0.02f, 0.5f, 0.97f, 0.999f, 1.0f};
    static const unsigned FL[6] = {0, EC_KC_LOCK, EC_KC_SOME, EC_KC_HOT, EC_KC_DIM | EC_KC_ACC, EC_KC_LOCK | EC_KC_HOT};
    const unsigned fl = FL[i % 6];
    ec_kit_cell_t c[4] = {{"CUTOFF", "2.4K", F[i % 6], fl | EC_KC_ACTUAL, "LFO1", F[(i + 3) % 6]},
                          {"LFO1>RESO", "+1%", D[i % 10], fl | EC_KC_DEPTH, 0, 0},
                          {"DEPTH", "-4%", D[(i + 5) % 10], (fl ^ EC_KC_SOME) | EC_KC_DEPTH | EC_KC_ACTUAL, 0, F[i % 6]},
                          {"A LONG VALUE", "AMEN_BREAK_170", F[(i + 1) % 6], fl, "2 MOD", 0}};
    ec_kit_header("T1", "FILTER/FILTER", "1", "P01", i & 1, 0);
    ec_kit_cells(c, 2, i & 1);
}
static void k_filter(int i)
{
    ec_kit_header("T1", "FILTER/FILTER", "1", "P01", 0, 0);
    ec_kit_filter_bars(20, 75, (float)(i * 23 % 128), (float)(i * 41 % 128), i % 4 + 1, i >= 4 ? (float)(i - 4) / 3.0f : -1,
                       (float)(i % 5 - 2) / 2.0f, i & 1);
}
static void k_env(int i)
{
    static const float V[6][6] = {{1, 2, 40, 89, 0, 60},  {2, 10, 0, 127, 30, 70}, {1, 24, 0, 70, 0, 50},
                                  {2, 0, 0, 0, 0, 0},     {2, 50, 0, 0, 128, 0},   {1, 127, 127, 127, 0, 128}};
    ec_kit_header("T1", "AMP/AMP ENV", "2", "P01", 0, 0);
    ec_kit_env_bars(20, 75, (int)V[i][0], V[i][1], V[i][2], V[i][3], V[i][4], V[i][5], (unsigned)(1 << (i % 5)));
}
static void k_grains(int seed)
{
    ec_grain_view_t g[EC_GRAIN_VIEW_MAX];
    uint32_t r = (uint32_t)seed * 2654435761u + 1;
    uint8_t ov[128];
    int k;
    for (k = 0; k < 128; k++)
        ov[k] = (uint8_t)((k * 37 + seed) % 256);
    for (k = 0; k < EC_GRAIN_VIEW_MAX; k++) {
        r = r * 1664525u + 1013904223u;
        g[k].pos = (float)(r >> 8) / 16777216.0f;
        g[k].len = 0.02f + 0.1f * (float)(k % 4);
        g[k].level = (float)(k % 5) / 4.0f;
        g[k].newest = k == 3;
    }
    ec_kit_header("T1", "LOOP/WARP", "3", "P01", 0, 0);
    ec_kit_grain_frame(2, 20, 116, 42, 44);
    ec_kit_wave(ov, 128, 4, 116, 23, 59, EC_C_L3, EC_C_L3, 0, 120);
    ec_kit_grains(g, EC_GRAIN_VIEW_MAX, 4, 116, 23);
    ec_kit_steps(64, 69, 0x9535, 0x0202, 0x0011, 0xC000, 4, 7);
}
static void k_settings(int i)
{
    static const ec_kit_row_t r[11] = {{"THEME", "PHOSPHOR", 0, 0, 1}, {"MODE POP-UP", "ON", 0, 0, 0},
                                       {"KNOB SPEED", "MED", 0, 0, 0}, {"AUTO-PREVIEW", "ON", 0, 0, 0},
                                       {"AFTERTOUCH", "ON", 1, 0, 0}, {"METRO", "WOOD", 0, "STOP", 0},
                                       {"METRO SOUND", "WOODBLOCK LONG", 0, 0, 0}, {"A", "1", 0, 0, 0},
                                       {"B", "2", 0, 0, 0}, {"C", "3", 0, 0, 0}, {"D", "4", 0, 0, 0}};
    ec_kit_header("T1", "APP/SETTINGS", "", "", 0, 1);
    ec_kit_list(r, 11, i, 12, EC_GH, 0);     /* 0.6.1: the list menu */
}
static void k_intro(int f) { ec_scr_intro(f); }
/* 0.6.1: the CAT stage, f = 0..23: the frame f / 2, playing at beat f * 0.37 (the beat and bar pulses), the last four
 * stopped; 0.6.3: every algorithm (f mod 7), each at its own seed and evolution time f * 9.7 */
static void k_cat(int f) { ec_scr_cat(f / 2, f * 0.37, f < 20, f * 9.7, f % ec_scr_cat_algos(), 1234u + (uint32_t)f); }
static void k_misc(int i)
{
    ec_kit_header("MST", "SEQ/PAT", "A", "Q 1 BAR", 0, i);
    ec_kit_key_ruler(66, 0x0020, 0x0FF0, 16);
    ec_kit_track_keys(46, 8, 0x5);
    ec_kit_playhead(40, 20, 58);
    ec_kit_arrows(10, 30, EC_C_ACC);
}

static void test_kit(void)
{
    int i;
    for (i = 0; i < 30; i++)
        kit_shot("kit_cells", k_cells, i);
    for (i = 0; i < 8; i++)
        kit_shot("kit_filter_bars", k_filter, i);
    for (i = 0; i < 6; i++)
        kit_shot("kit_env_bars", k_env, i);
    for (i = 0; i < 3; i++)
        kit_shot("kit_grains", k_grains, i);
    for (i = 0; i < 11; i += 5)
        kit_shot("kit_settings", k_settings, i);
    for (i = 0; i < ec_scr_intro_frames(); i++)
        kit_shot("kit_intro", k_intro, i);
    for (i = 0; i < 2 * ec_scr_cat_frames(); i++)
        kit_shot("kit_cat", k_cat, i);
    for (i = 0; i < 2; i++)
        kit_shot("kit_misc", k_misc, i);
    CHECK(ec_scr_intro_frames() == 55 && ec_scr_cat_frames() == 12, "the sprites: %d intro, %d cat frames",
          ec_scr_intro_frames(), ec_scr_cat_frames());
}

/* the fine check finds what it should: a 1-unit bar, rule, outline and 1-unit runs of a fill (between two bars, under one); not
 * text, a sprite, 2-unit lines, nor BG gaps */
static int fine_of(void (*draw)(void))
{
    char where[80];
    ec_gfx_begin(g_kitlcd);
    ec_gfx_fill(EC_C_BG);
    draw();
    ec_gfx_begin(ec_plat_lcd());
    return fine_runs(where, sizeof where);
}
static void f_bar1(void) { ec_gfx_rect(10, 10, 20, 1, EC_C_L8); }
static void f_rule1(void) { ec_gfx_rect(60, 20, 1, 30, EC_C_L3); }
static void f_frame1(void)
{
    ec_gfx_rect(10, 10, 30, 20, EC_C_L8);
    ec_gfx_rect(11, 11, 28, 18, EC_C_BG);
}
static void f_gap1(void)
{
    ec_gfx_rect(10, 10, 30, 6, EC_C_LOCK);
    ec_gfx_rect(10, 10, 30, 2, EC_C_L8);
    ec_gfx_rect(10, 13, 30, 2, EC_C_L8);
}
static void f_text(void)
{
    ec_gfx_text(4, 10, "HELLO WORLD 1234567890 -_=", 1, EC_C_L15);
    ec_gfx_rect(0, 30, EC_GW, 12, EC_C_LOCK);
    ec_gfx_text(4, 31, "LOCKED 12", 2, EC_C_BG);
    ec_gfx_rect(4, 42, 50, 2, EC_C_BG);
}
static void f_ok(void)
{
    ec_gfx_rect(10, 10, 20, 2, EC_C_L8);
    ec_gfx_frame(40, 40, 30, 20, EC_C_L12);
    ec_gfx_line(5, 70, 100, 90, EC_C_ACC);
    ec_gfx_dots_h(0, 120, 100, 4, EC_C_L3);
    ec_gfx_rect(10, 110, 3, 6, EC_C_L8), ec_gfx_rect(14, 110, 3, 6, EC_C_L8);   /* a 1-unit BG gap */
}
static void f_sprite(void)
{
    static const uint8_t spr[16] = {0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50,
                                    0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50};   /* a 1-unit column */
    ec_gfx_blit4(spr, 2, 16, 30, 30, 0);
}
static void test_fine_check(void)
{
    CHECK(fine_of(f_bar1) == 1 && fine_of(f_rule1) == 1 && fine_of(f_frame1) == 4 && fine_of(f_gap1) == 2,
          "the fine check finds a 1-unit bar (%d), rule (%d), outline (%d), fill run (%d)", fine_of(f_bar1),
          fine_of(f_rule1), fine_of(f_frame1), fine_of(f_gap1));
    CHECK(!fine_of(f_text) && !fine_of(f_ok) && !fine_of(f_sprite),
          "the fine check passes text (%d), 2-unit lines and BG gaps (%d), a sprite (%d)", fine_of(f_text),
          fine_of(f_ok), fine_of(f_sprite));
}

/* ------------------------------------------------------------------------------------------- the palette --- */
/* 0.6.1 (docs/UX-V2.md §10): SRC is each theme's blue (blue its strongest channel) and stays clear of ACC, LOCK and
 * ALERT (RGB distance >= 150), of the ground (>= 120: a fill you can read text on) and of every other slot (>= 60) */
static double rgb_dist(uint32_t a, uint32_t b)
{
    const double dr = (double)(a >> 16 & 255) - (b >> 16 & 255), dg = (double)(a >> 8 & 255) - (b >> 8 & 255),
                 db = (double)(a & 255) - (b & 255);
    return sqrt(dr * dr + dg * dg + db * db);
}
static void test_palette(void)
{
    int t, k;
    for (t = 0; t < EC_NTHEME; t++) {
        const uint32_t *p = ec_themes[t], s = p[EC_C_SRC];
        const int r = s >> 16 & 255, g = s >> 8 & 255, b = s & 255;
        CHECK(b > r && b > g, "%s: SRC %06X is blue", ec_theme_names[t], (unsigned)s);
        for (k = 0; k < EC_NPAL; k++) {                /* 0.6.1 (owner): pure black, never a near-black */
            const uint32_t c = p[k];
            const int cr = c >> 16 & 255, cg = c >> 8 & 255, cb = c & 255;
            CHECK(!c || cr > 0x20 || cg > 0x20 || cb > 0x20, "%s: %s %06X is near-black (all channels <= 0x20)",
                  ec_theme_names[t], ec_pal_names[k], (unsigned)c);
        }
        for (k = 0; k < EC_NPAL; k++) {
            const double min = k == EC_C_ACC || k == EC_C_LOCK || k == EC_C_ALERT ? 150 : k == EC_C_BG ? 120 : 60;
            if (k != EC_C_SRC)
                CHECK(rgb_dist(s, p[k]) >= min, "%s: SRC %06X vs %s %06X: distance %.0f < %.0f", ec_theme_names[t],
                      (unsigned)s, ec_pal_names[k], (unsigned)p[k], rgb_dist(s, p[k]), min);
        }
    }
}

/* ------------------------------------------------------------------------------------------- the app --- */
static void every_page(const char *tag)
{
    int cat, pg;
    char name[64];
    for (cat = 0; cat < EC_NCAT; cat++) {
        const int n = ec_cat_pages(cat, ec_app_params(A));
        for (pg = 0; pg < n; pg++) {
            A->cat = cat;
            A->page[cat] = pg;
            snprintf(name, sizeof name, "%s_%s_%d", tag, ec_cat_name(cat), pg + 1);
            shot(name);
        }
    }
}

/* 0.6.1 (docs/UX-V2.md §10): the frames that showed the owner's 1-unit depth bar, which 0.6.0's set never had (no
 * route on a shown param, no held source key, no lock on a depth). On every page of every mode: the page's own params
 * modulated (sources LFO 1 / LFO 2 / MOD ENV / VEL by turns, depths tiny to full), then a held source key (its depth
 * bars in SRC), then every lockable param of the page (and the routes' depths) locked on step 1 with step 1 held (the
 * LOCK cells), not held (the outlines), the depth locks under a held source, and no locks (plain). The track's params
 * and step 1 come back after. */
static const float DEPTHS[8] = {1, -1, 4, -50, 100, -100, 96, 2};
static void route_page(float *v, const ec_page_t *pg)
{
    static const int SRC[4] = {EC_MS_LFO1, EC_MS_LFO2, EC_MS_ENV, EC_MS_VEL};
    int i, k = 0;
    for (i = 0; i < EC_MTX_SLOTS; i++) {
        v[ec_mtx_src_id(i)] = EC_MS_OFF + 1;
        v[ec_mtx_dst_id(i)] = 0;
        v[ec_mtx_depth_id(i)] = 0;
    }
    for (i = 0; i < pg->n && k < EC_MTX_SLOTS; i++) {
        ec_item_t it;
        ec_mk_item(&pg->it[i], &it);
        if (it.kind != EC_IT_PARAM || !ec_param_mod_dest(it.id))
            continue;
        v[ec_mtx_src_id(k)] = (float)(SRC[k % 4] + 1);
        v[ec_mtx_dst_id(k)] = (float)(it.id + 1);
        v[ec_mtx_depth_id(k)] = DEPTHS[k];
        k++;
    }
}
static void lock_page(ec_step_t *st, const float *v, const ec_page_t *pg)
{
    int i;
    st->nlock = 0;
    for (i = 0; i < pg->n; i++) {
        ec_item_t it;
        ec_mk_item(&pg->it[i], &it);
        if (it.kind == EC_IT_PARAM && (EC_PARAMS[it.id].flags & EC_PF_LOCKABLE))
            ec_step_lock_set(st, it.id, EC_PARAMS[it.id].min + (EC_PARAMS[it.id].max - EC_PARAMS[it.id].min) * 0.7f);
    }
    for (i = 0; i < EC_MTX_SLOTS; i++)
        if (ec_mtx_src(v, i) && ec_mtx_dst(v, i) >= 0)
            ec_step_lock_set(st, ec_mtx_depth_id(i), -DEPTHS[i] / 2);
}
static void mod_lock_page(const char *tag)
{
    ec_ptrack_t *tr = ec_project_track(&A->proj, A->track);
    float *v = tr->v;
    ec_page_t pg;
    char name[80];
    ec_app_page(A, &pg);
    route_page(v, &pg);
    snprintf(name, sizeof name, "%s_mod", tag);
    shot(name);
    A->mk.held = EC_MK_LFO1;
    snprintf(name, sizeof name, "%s_mod_held_lfo1", tag);
    shot(name);
    A->mk.held = EC_MK_LFO2;
    snprintf(name, sizeof name, "%s_mod_held_lfo2", tag);
    shot(name);
    A->mk.held = -1;
    lock_page(&tr->steps[0], v, &pg);
    tr->steps[0].trig = 1;
    A->step_page = 0;
    A->step_held = 1;
    snprintf(name, sizeof name, "%s_lock_held", tag);
    shot(name);
    A->mk.held = EC_MK_LFO1;
    snprintf(name, sizeof name, "%s_lock_held_mod_held", tag);
    shot(name);
    A->step_held = 0;
    snprintf(name, sizeof name, "%s_lock_outline_mod_held", tag);
    shot(name);
    A->mk.held = -1;
    snprintf(name, sizeof name, "%s_lock_outline", tag);
    shot(name);
    tr->steps[0].nlock = 0;
    snprintf(name, sizeof name, "%s_lock_none", tag);
    shot(name);
}
static void test_mod_lock(void)
{
    ec_ptrack_t *tr = ec_project_track(&A->proj, A->track);
    static float v0[EC_NP];
    const ec_step_t s0 = tr->steps[0];
    char name[64];
    int cat, pg, m;
    memcpy(v0, tr->v, sizeof v0);
    reset_state();
    for (cat = 0; cat < EC_NCAT; cat++) {
        const int n = ec_cat_pages(cat, tr->v);
        for (pg = 0; pg < n; pg++) {
            A->cat = cat;
            A->page[cat] = pg;
            snprintf(name, sizeof name, "ml_%s_%d", ec_cat_name(cat), pg + 1);
            mod_lock_page(name);
        }
    }
    for (m = EC_NCAT; m < EC_NMODE; m++) {               /* the fixed mode screens (KEYBOARD, PAGES, ..) */
        A->scr = m;
        snprintf(name, sizeof name, "ml_mode_%d", m);
        mod_lock_page(name);
    }
    A->scr = 0;
    A->seq.playing = 1;                                   /* playing: the actual marks beside the bars */
    run(200);
    A->cat = EC_CAT_FILT;
    A->page[EC_CAT_FILT] = 0;
    mod_lock_page("ml_playing_filter");
    A->seq.playing = 0;
    memcpy(tr->v, v0, sizeof v0);
    tr->steps[0] = s0;
    A->mk.held = -1;
    reset_state();
}

/* v61-fx and later: more frames (the FX type menu, the LOOP ENGINE quick menus) go here; shot(name) checks a frame of
 * the state set up, in every theme */
static void test_more_frames(void) {}

/* 0.6.0: the mode screens (src/ui/ec_scr_modes.c) and the mode pop-up, by setting the panel's state */
static void test_modes(void)
{
    float *v = ec_app_params(A);
    const float mach0 = v[EC_P_MACHINE];
    char name[48];
    int m, f;
    reset_state();
    for (m = 0; m < EC_NMODE; m++) {                       /* every mode's screen (0.6.1: no mode pop-up) */
        if (m < EC_NCAT) {
            A->scr = 0;
            A->cat = m;
        } else
            A->scr = m;
        snprintf(name, sizeof name, "mode_%d", m);
        shot(name);
    }
    A->scr = EC_MODE_KEYBOARD;                             /* KEYBOARD: keys, then a slice track with a target */
    A->pn.oct = -1;
    shot("keyboard_keys");
    v[EC_P_MACHINE] = EC_MACH_GRID_SLICE;
    A->pn.target = 5;
    A->pn.kb_bank = 1;
    shot("keyboard_slices");
    v[EC_P_MACHINE] = mach0;
    A->pn.target = 0;
    A->pn.oct = 0;
    A->scr = EC_MODE_PAGES;                                /* PAGES: a 64-step pattern, page 3 */
    v[EC_P_PATTERN_STEPS] = 64;
    A->step_page = 2;
    shot("pages_64");
    v[EC_P_PATTERN_STEPS] = 16;
    A->step_page = 0;
    A->scr = 0;
    A->cat = EC_CAT_XFD;                                   /* CROSSFADER gliding */
    A->e5.gliding = 1;
    A->e5.glide_target = 0.8f;
    A->pn.xf_key = 9;
    shot("xfd_glide");
    A->e5.gliding = 0;
    A->pn.xf_key = -1;
    A->cat = EC_CAT_PAT;                                   /* PATTERN LAUNCH, bank C */
    A->pn.bank = 2;
    shot("patterns_c");
    A->pn.bank = 0;
    A->cat = EC_CAT_MASTER;                                /* 0.6.1: MIX with varied pans, T3 / S1 selected */
    {
        static const float PAN[EC_NTRACK] = {0, 128, 64, 32, 96, 70};
        float p0[EC_NTRACK];
        const int mpg0 = A->page[EC_CAT_MASTER];
        for (f = 0; f < EC_NTRACK; f++) {
            p0[f] = A->proj.pat[A->proj.current].tr[f].v[EC_P_PAN];
            A->proj.pat[A->proj.current].tr[f].v[EC_P_PAN] = PAN[f];
        }
        for (f = 0; f < ec_cat_pages(EC_CAT_MASTER, v); f++)
            if (ec_page_pic(EC_CAT_MASTER, f, 0) == EC_PIC_MIX)
                A->page[EC_CAT_MASTER] = f;
        A->track = 2;
        shot("mix_pans");
        A->bus = 1;
        shot("mix_pans_s1");
        A->bus = 0;
        A->track = 0;
        A->page[EC_CAT_MASTER] = mpg0;
        for (f = 0; f < EC_NTRACK; f++)
            A->proj.pat[A->proj.current].tr[f].v[EC_P_PAN] = p0[f];
    }
    A->cat = EC_CAT_MASTER;                                /* MIX's CAT, a few frames */
    A->pn.cat = 1;
    for (f = 0; f < 12; f += 5) {
        A->ms += 100;
        snprintf(name, sizeof name, "cat_%d", f);
        shot(name);
    }
    A->pn.cat = 0;
    A->pn.as_open = 1;                                     /* APP SETTINGS, the cursor on row 6 */
    A->pn.as_row = 6;
    shot("app_settings");
    A->pn.as_row = 0;                                      /* 0.6.1: THEME selected (two lines) */
    shot("app_settings_theme");
    A->pn.as_open = 0;
    A->cat = EC_CAT_FILE;                                  /* 0.6.0 part 2: FILE's browser (previewing), the editor */
    A->page[EC_CAT_FILE] = 0;
    A->pn.pv_play = 1;
    shot("file_browser");
    A->pn.pv_play = 0;
    for (f = 0; f < ec_cat_pages(EC_CAT_FILE, v); f++)
        if (ec_page_pic(EC_CAT_FILE, f, 0) == EC_PIC_EDITOR)
            A->page[EC_CAT_FILE] = f;
    A->pn.ed_zoom = 3;
    A->pn.ed_view = 0.3f;
    A->pn.ed_point = 1;
    shot("file_editor_8x");
    A->pn.ed_zoom = 0;
    A->pn.ed_view = 0;
    A->pn.ed_point = 0;
    shot("file_editor_1x");
    for (f = 0; f < ec_cat_pages(EC_CAT_FILE, v); f++)    /* 0.6.1: POOL, a slot carried for MOVE */
        if (ec_page_pic(EC_CAT_FILE, f, 0) == EC_PIC_POOL)
            A->page[EC_CAT_FILE] = f;
    A->pn.pool_cur = 2;
    shot("file_pool");
    A->pn.pool_carry = 0;
    shot("file_pool_move");
    A->pn.pool_carry = -1;
    A->pn.pool_cur = 0;
    A->page[EC_CAT_FILE] = 0;
    A->scr = EC_MODE_SAMPLER;                              /* the SAMPLER: idle, armed, taking */
    A->rec.in_meter[0] = 0.5f;
    A->rec.in_meter[1] = 0.95f;
    shot("sampler_idle");
    A->rec.state = EC_RS_ARMED;
    shot("sampler_armed");
    A->rec.state = EC_RS_RUN;
    A->rec.n = 100000;
    shot("sampler_taking");
    A->rec.state = EC_RS_IDLE;
    A->rec.n = 0;
    A->rec.in_meter[0] = A->rec.in_meter[1] = 0;
    A->scr = 0;
    A->cat = EC_CAT_FILT;
    A->pn.rs_open = 1;                                     /* the record settings, the cursor on AUTOMATION */
    A->pn.rs_row = 5;
    shot("rec_settings");
    A->pn.rs_open = 0;
    ec_app_alert("STOP FIRST");                            /* an alert in the header */
    shot("alert");
    A->msg_ms = 0;
    A->pn.intro = 1;                                       /* the intro */
    for (f = 0; f < ec_scr_intro_frames(); f += 9) {
        A->pn.intro_frame = f;
        snprintf(name, sizeof name, "intro_%d", f);
        shot(name);
    }
    reset_state();
    A->cat = EC_CAT_SRC;
}

static void test_app(void)
{
    float *v = ec_app_params(A);
    const float mach0 = v[EC_P_MACHINE], mode0 = v[EC_P_MODE];
    char name[64];
    int cat, b, m;
    ec_page_t pg;
    reset_state();
    every_page("page");
    for (cat = 0; cat < EC_NCAT; cat++) {                     /* the settings lists */
        A->cat = cat;
        A->settings = ec_settings_items(cat, v, &pg) != 0;
        if (A->settings) {
            snprintf(name, sizeof name, "setup_%s", ec_cat_name(cat));
            A->ux.set_row = 0;
            shot(name);
            A->ux.set_row = 99;                               /* 0.6.1: the last row selected (scrolled) */
            snprintf(name, sizeof name, "setup_%s_last", ec_cat_name(cat));
            shot(name);
            A->ux.set_row = 0;
        }
    }
    reset_state();
    for (b = 1; b <= 3; b++) {                                /* the buses: S1 S2 MST */
        A->bus = b;
        every_page(b == 3 ? "mst" : b == 1 ? "s1" : "s2");
    }
    reset_state();
    /* a held step with locks: the header's count, the locked cells in LOCK */
    {
        ec_ptrack_t *tr = ec_project_track(&A->proj, A->track);
        tr->steps[0].trig = 1;
        ec_step_lock_set(&tr->steps[0], EC_P_FILTER_CUTOFF, 30);
        ec_step_lock_set(&tr->steps[0], EC_P_FILTER_RES, 90);
        ec_step_lock_set(&tr->steps[0], EC_P_PITCH, 70);
        ec_step_lock_set(&tr->steps[0], EC_P_ENV_ATTACK, 20);
        A->step_page = 0;
        A->step_held = 1;
        every_page("held");
        A->step_held = 0;
    }
    /* the machines on SRC (slices, razor, neighbor, input) and every warp on WARP */
    A->cat = EC_CAT_SRC;
    for (m = EC_MACH_LOOP; m <= EC_MACH_INPUT; m++) {
        v[EC_P_MACHINE] = (float)m;
        A->track = m == EC_MACH_NEIGHBOR ? 1 : 0;
        v = ec_app_params(A);
        v[EC_P_MACHINE] = (float)m;
        run(30);
        for (A->page[EC_CAT_SRC] = 0; A->page[EC_CAT_SRC] < ec_cat_pages(EC_CAT_SRC, v); A->page[EC_CAT_SRC]++) {
            snprintf(name, sizeof name, "machine%d_src_%d", m, A->page[EC_CAT_SRC] + 1);
            shot(name);
        }
        A->page[EC_CAT_SRC] = 0;
        shot("loop_keys");
        v[EC_P_MACHINE] = mach0;
        A->track = 0;
        v = ec_app_params(A);
    }
    v[EC_P_MACHINE] = mach0;
    for (m = 1; m <= 13; m++) {
        v[EC_P_MODE] = (float)m;
        run(20);
        every_page("warp");
    }
    v[EC_P_MODE] = mode0;
    /* the pop-ups and overlays */
    A->cat = EC_CAT_SRC;
    A->page[EC_CAT_SRC] = 0;
    A->ux.f3 = 1;
    shot("f3");
    A->ux.f3 = 0;
    A->e5.naming = EC_NM_PATTERN;
    shot("naming_pattern");
    A->e5.naming = EC_NM_PROJECT;
    shot("naming_project");
    A->e5.naming = 0;
    A->rec.open = 1;
    shot("rec");
    A->rec.row = EC_REC_ROW_SYNC;                          /* 0.6.1: the setup's list scrolled to its last row */
    shot("rec_sync");
    A->rec.row = 0;
    A->rec.open = 0;
    test_modes();
    test_mod_lock();
    test_more_frames();
    A->ux.to_ms = A->ms ? A->ms : 1;
    A->ux.to_kind = EC_TO_SAMPLE;
    snprintf(A->ux.to_label, sizeof A->ux.to_label, "SAMPLE");
    snprintf(A->ux.to_val, sizeof A->ux.to_val, "AMEN_BREAK_170_LONG_NAME.WAV");
    snprintf(A->ux.to_sub, sizeof A->ux.to_sub, "3/12");
    A->ux.to_frac = 0.4f;
    shot("takeover_sample");
    A->ux.to_kind = 0;
    snprintf(A->ux.to_val, sizeof A->ux.to_val, "L20");
    shot("takeover_value");
    A->ux.to_ms = 0;
    A->fn = 1;
    A->pn.shift_held = 1;
    A->ux.shift_used = 0;
    A->ux.shift_ms = 0;
    A->ms += 5000;
    shot("shift_map");
    A->pn.shift_held = 0;
    A->pn.shift_sticky = 1;
    shot("shift_sticky");
    A->pn.shift_sticky = 0;
    A->fn = 0;
    snprintf(A->msg, sizeof A->msg, "CLEAR?");                /* a question: in the header (0.6.1: no toast box) */
    A->msg_ms = A->ms;
    A->e5.confirm = 1;
    shot("question");
    reset_state();
    snprintf(A->msg, sizeof A->msg, "A LONG MESSAGE IN THE HEADER");
    A->msg_ms = A->ms;
    shot("message");
    reset_state();
    A->seq.playing = 1;                                        /* playing: the playhead, the dim cells */
    run(200);
    every_page("playing");
    reset_state();
}

/* 0.6.1 (v61-fx): FX's INSERT page with both slots, its P2, the type menu, a LOOP ENGINE quick menu */
static void test_v61_fx(void)
{
    float *v = ec_app_params(A);
    const float m1 = v[EC_P_FX_MACHINE], m2 = v[EC_P_INS2_MACHINE];
    int p;
    reset_state();
    A->cat = EC_CAT_FX;
    for (p = 0; p < ec_cat_pages(EC_CAT_FX, v) && ec_page_pic(EC_CAT_FX, p, 0) != EC_PIC_FX; p++)
        ;
    A->page[EC_CAT_FX] = p;
    v[EC_P_FX_MACHINE] = EC_FX_REVERB;
    v[EC_P_INS2_MACHINE] = EC_FX_BLACKHOLE;
    ec_ins2_reset(v);
    A->pn.ins_slot = 1;
    A->pn.ins_page = 0;
    run(30);
    shot("fx_insert_two");
    A->pn.ins_page = 1;
    shot("fx_insert_p2");
    A->pn.ins_page = 0;
    ec_hm_arm(1, EC_HM_INS2, 0, "INS 2 TYPE");
    ec_hm_knob(EC_E_K1, 3);                                    /* (shows it at once) */
    shot("fx_type_menu");
    ec_hm_knob(EC_E_K1, 12);
    shot("fx_type_menu_end");
    ec_hm_cancel();
    A->cat = EC_CAT_SRC;
    ec_hm_arm(3, EC_HM_PARAM, EC_P_MODE, "WARP MODE");
    ec_hm_knob(EC_E_SELECT, 2);
    shot("loop_warp_menu");
    ec_hm_cancel();
    v[EC_P_FX_MACHINE] = m1;
    v[EC_P_INS2_MACHINE] = m2;
    A->pn.ins_slot = 0;
    reset_state();
}

int main(int argc, char **argv)
{
    int t;
    if (argc < 2) {
        printf("usage: ec_test_grid <folder> [png dir]\n");
        return 2;
    }
    g_png = argc > 2 ? argv[2] : 0;
    g_who_dump = getenv("EC_GRID_DUMP") != 0;          /* every bad run, per CREAM frame (for finding them) */
    if (g_who_dump)
        printf("MAIN %p\n", (void *)main);
    ec_fs_native_root(argv[1], "elasticat");
    C = fm1core_get(FM1CORE_ABI);
    if (!C)
        return 1;
    P = C->panel;
    C->flash_stage(NULL, 0);
    C->options(FM1CORE_FLASH_STAGED, 0, 0, 1);
    P->ready = 2;
    C->init(0);
    run(1500);                                                /* the scan, T1's first file */
    CHECK(ec_gfx_themes() == 6 && !strcmp(ec_gfx_theme_name(0), "CREAM") && !strcmp(ec_gfx_theme_name(5), "PAPER") &&
              !ec_gfx_theme_name(6)[0],
          "the six themes");
    test_palette();
    test_fine_check();
    for (t = 0; t < ec_gfx_themes(); t++) {
        int before = shots;
        g_theme = t;
        ec_gfx_set_theme(t);
        CHECK(ec_gfx_theme() == t, "set_theme %d", t);
        test_app();
        test_v61_fx();
        test_kit();
        if (t == 0)
            printf("test_grid: %d frames per theme\n", shots - before);
    }
    ec_gfx_set_theme(99);
    CHECK(ec_gfx_theme() == EC_NTHEME - 1, "set_theme clamps");
    ec_gfx_set_theme(EC_TH_CREAM);
    C->shutdown();
    printf("test_grid: %d frames, %d with the fine check (%d with param cells; %d MIX / CAT frames skipped), %d checks, "
           "%d failed\n", shots, fine_frames, fine_cells, fine_skipped, checks, fails);
    return fails ? 1 : 0;
}
