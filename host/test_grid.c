/* SPDX-License-Identifier: GPL-3.0-only */
/* test_grid: the owner's graphics rules (docs/UX-V2.md §7) on the firmware's own screens. With the demo folder
 * (host/make_demo_folder.py) booted through the core, a broad set of screens is reached by setting the app state
 * directly (category, page, settings, the pop-ups, F3, naming, the scene page, REC, the takeover, SHIFT's map, toasts,
 * held steps with locks, the machines and warps), not by gestures; plus the kit's parts (ec_ui_kit.h) drawn alone:
 * the mode pop-up, the norns filter / envelope bars, the grains, the intro and the CAT frames. For every LCD frame,
 * in each of the six themes: every 2x2 block is one colour, and every colour is one of the theme's 16.
 *   ec_test_grid <folder> [png dir]     (png dir: the CREAM frames, for a look; results are not committed) */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_fs_native.h"
#include "ec_gfx.h"
#include "ec_png.h"
#include "ec_screens.h"
#include "ec_ui_kit.h"
#include "fm1core.h"

static int fails, checks, shots;
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

/* the rules on one LCD frame */
static void check_lcd(const uint16_t *lcd, const char *name)
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
    A->redraw = 1;
    C->frame();
    check_lcd(P->lcd, name);
}

/* the kit alone: draw(), presented to a buffer of our own */
static void kit_shot(const char *name, void (*draw)(int), int arg)
{
    ec_gfx_begin(g_kitlcd);
    ec_gfx_fill(EC_C_BG);
    draw(arg);
    ec_gfx_present();
    ec_gfx_begin(ec_plat_lcd());
    check_lcd(g_kitlcd, name);
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
    A->e5.toast_ms = 0;
    A->e5.toast_keep = 0;
    A->patn_open = 0;
    A->rec.open = 0;
    A->ux.to_ms = 0;
    A->fn = 0;
    A->msg_ms = 0;
    A->keymode = EC_KM_STEPS;
    A->sel_held = 0;
}

/* ------------------------------------------------------------------------------------------- the kit --- */
static void k_popup(int i)
{
    static const char *const NM[3] = {"LOOP ENGINE", "FX", "MODULATION"};
    static const char *const RL[3] = {"PPPPPSSSSSX", "PAXOSSSSSSX", "PSSSSSSSSSX"};
    ec_kit_cell_t c[4] = {{"CUTOFF", "2.4K", 0.6f, 0, "LFO1", 0}, {"RESO", "35", 0.3f, EC_KC_LOCK, 0, 0},
                          {"DEPTH", "+25%", 0.25f, EC_KC_DEPTH | EC_KC_ACTUAL, 0, 0.4f},
                          {"TYPE", "LP", 0, EC_KC_SOME | EC_KC_ACC, 0, 0}};
    ec_kit_header("T1", "LOOP/LOOP", "1", "P01", i, 0);
    ec_kit_cells(c, 3, i % 3);
    ec_kit_popup(NM[i], "PAGE", RL[i], i - 1);
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
    static const ec_kit_row_t r[11] = {{"THEME", "CREAM", 0, 0}, {"MODE POP-UP", "ON", 0, 0}, {"KNOB SPEED", "MED", 0, 0},
                                       {"AUTO-PREVIEW", "ON", 0, 0}, {"AFTERTOUCH", "ON", 1, 0}, {"METRO", "WOOD", 0, "STOP"},
                                       {"MIDI CC", "1+11", 0, 0}, {"A", "1", 0, 0}, {"B", "2", 0, 0}, {"C", "3", 0, 0},
                                       {"D", "4", 0, 0}};
    ec_kit_header("T1", "APP/SETTINGS", "", "", 0, 1);
    ec_kit_settings(r, 11, i / 4, i);
}
static void k_intro(int f) { ec_scr_intro(f); }
static void k_cat(int f)
{
    ec_scr_cat(f);
    ec_kit_header("T2", "MIX/CAT", "", "120 BPM 1X", 0, 0);
    ec_kit_track_keys(99, 1, 0x10);
}
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
    for (i = 0; i < 3; i++)
        kit_shot("kit_popup", k_popup, i);
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
    for (i = 0; i < ec_scr_cat_frames(); i++)
        kit_shot("kit_cat", k_cat, i);
    for (i = 0; i < 2; i++)
        kit_shot("kit_misc", k_misc, i);
    CHECK(ec_scr_intro_frames() == 55 && ec_scr_cat_frames() == 12, "the sprites: %d intro, %d cat frames",
          ec_scr_intro_frames(), ec_scr_cat_frames());
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
            shot(name);
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
        A->keymode = EC_KM_LOOP;
        A->page[EC_CAT_SRC] = 0;
        shot("loop_keys");
        A->keymode = EC_KM_STEPS;
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
    A->rec.open = 0;
    A->patn_open = 1;
    shot("patn");
    A->patn_open = 0;
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
    A->ux.shift_used = 0;
    A->ux.shift_ms = 0;
    A->ms += 5000;
    shot("shift_map");
    A->fn = 0;
    snprintf(A->e5.toast, sizeof A->e5.toast, "COPIED 4 STEPS");
    A->e5.toast_ms = A->ms;
    shot("toast");
    A->e5.toast_keep = 1;
    snprintf(A->e5.toast, sizeof A->e5.toast, "CLEAR?");
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

int main(int argc, char **argv)
{
    int t;
    if (argc < 2) {
        printf("usage: ec_test_grid <folder> [png dir]\n");
        return 2;
    }
    g_png = argc > 2 ? argv[2] : 0;
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
    for (t = 0; t < ec_gfx_themes(); t++) {
        int before = shots;
        g_theme = t;
        ec_gfx_set_theme(t);
        CHECK(ec_gfx_theme() == t, "set_theme %d", t);
        test_app();
        test_kit();
        if (t == 0)
            printf("test_grid: %d frames per theme\n", shots - before);
    }
    ec_gfx_set_theme(99);
    CHECK(ec_gfx_theme() == EC_NTHEME - 1, "set_theme clamps");
    ec_gfx_set_theme(EC_TH_CREAM);
    C->shutdown();
    printf("test_grid: %d frames, %d checks, %d failed\n", shots, checks, fails);
    return fails ? 1 : 0;
}
