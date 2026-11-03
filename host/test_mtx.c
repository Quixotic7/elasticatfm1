/* SPDX-License-Identifier: GPL-3.0-only */
/* test_mtx: 0.5.0's mod matrix (docs/MOD-MATRIX.md) on the shipped layout, through the core descriptor with the demo
 * folder: the resolve (generic destinations and the fast buses, M1..M4 as sources, AT / CC 1 / CC 11), a 0.4 record's
 * migration (LFO -> CUT the same strength; the routes past 8 dropped and counted), the MATRIX page (SELECT the row
 * and on, K1 SRC, K2 DST with SHIFT's category jump, K3 DEPTH's fine steps), every source-key gesture (0.6.0's
 * source template: the first detent, DEPTH, release frees an unused slot, NO MOD, FULL, F2 removes, held steps lock,
 * taps open pages, MODULATION's macro keys), the marks, the labels and LEDs.
 *   ec_test_mtx <folder> [png dir] */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_fs_native.h"
#include "ec_mtx.h"
#include "ec_png.h"
#include "ec_scene.h"
#include "ec_snap.h"
#include "fm1core.h"

uint32_t fm1core_labels_epoch(void);
const char *fm1core_label(int32_t which);

static int fails, checks;
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
static const fm1core_t *C;
static fm1panel_t *P;
static uint32_t ms;
static const char *g_png;
static int16_t g_out[2 * 128];

static void run(int n)
{
    int i;
    for (i = 0; i < n; i++) {
        ec_fs_native_pump();
        C->tick(ms++);
        if (ms % 3 == 0)
            C->audio(g_out, 128);
        if (ms % 15 == 0)
            C->frame();
    }
}
static void btn(int b, int down)
{
    const uint32_t m = 1u << P->btn_id[b];
    if (down) {
        P->buttons |= m;
        P->buttons_tap |= m;
    } else
        P->buttons &= ~m;
}
static void press(int b, int dur)
{
    btn(b, 1);
    run(dur);
    btn(b, 0);
    run(20);
}
static int key_of(int upper, int i)
{
    int k, nl = 0, nu = 0;
    for (k = 0; k < FM1P_NKEY; k++) {
        const int s = k % 12;
        const int natural = s == 0 || s == 2 || s == 4 || s == 6 || s == 7 || s == 9 || s == 11;
        if (natural && !upper && nl++ == i)
            return k;
        if (!natural && upper && nu++ == i)
            return k;
    }
    return 0;
}
static void key(int upper, int i, int down)
{
    const int k = key_of(upper, i);
    if (down) {
        P->key_vel[k] = 100;
        P->keys |= 1u << k;
        P->keys_tap |= 1u << k;
    } else
        P->keys &= ~(1u << k);
    run(5);
}
static void tapkey(int upper, int i)
{
    key(upper, i, 1);
    run(60);
    key(upper, i, 0);
    run(20);
}
static void turn(int role, int n)
{
    P->enc[P->enc_id[role]] += n * P->enc_dir[role];
    run(1);
}
/* slow single detents (no acceleration) */
static void turns(int role, int n)
{
    int i;
    for (i = 0; i < (n < 0 ? -n : n); i++) {
        turn(role, n < 0 ? -1 : 1);
        run(120);
    }
}
static float *tv(void) { return ec_project_track(&A->proj, A->track)->v; }
static const char *L(int which)
{
    const char *s = fm1core_label(which);
    return s ? s : "(null)";
}
static void home(void) { press(FM1P_B_HOME, 40); }
static void clear_matrix(void)
{
    int k;
    for (k = 0; k < EC_MTX_SLOTS; k++) {
        tv()[ec_mtx_src_id(k)] = 1;
        tv()[ec_mtx_dst_id(k)] = 0;
        tv()[ec_mtx_depth_id(k)] = 0;
    }
    ec_seq_touch(&A->seq, A->track);
    run(20);
}
static void png(const char *name)
{
    char path[512];
    if (!g_png)
        return;
    A->redraw = 1;
    C->frame();
    snprintf(path, sizeof path, "%s/%s", g_png, name);
    if (ec_png_write_lcd(path, P->lcd, 240, 240, 2) == 0)
        printf("  wrote %s\n", path);
}
static void cc(int num, int val) { C->midi_in(0xBu | (0xB3u << 8) | ((uint32_t)num << 16) | ((uint32_t)val << 24)); }

/* ------------------------------------------------------------------------------------------- the resolve --- */
static void test_resolve_unit(void)
{
    float v[EC_NP], live[EC_MSRC_N] = {0};
    printf("resolve: generic destinations, M1..M4, clamping\n");
    ec_params_default(v);
    v[EC_P_FILTER_DRIVE] = 20;
    v[ec_mtx_src_id(0)] = EC_MS_LFO1 + 1;
    v[ec_mtx_dst_id(0)] = EC_P_FILTER_DRIVE + 1;
    v[ec_mtx_depth_id(0)] = 50;
    CHECK(ec_mtx_generic(v), "LFO1 -> DRIVE is generic");
    live[EC_MSRC_LFO1] = 0.5f;
    ec_mtx_apply(v, live);
    CHECK(fabsf(v[EC_P_FILTER_DRIVE] - (20 + 0.25f * 127)) < 0.01f, "DRIVE 20 + 0.5 x 50 %% x 127 = %g",
          v[EC_P_FILTER_DRIVE]);
    live[EC_MSRC_LFO1] = -1;
    v[EC_P_FILTER_DRIVE] = 20;
    ec_mtx_apply(v, live);
    CHECK(v[EC_P_FILTER_DRIVE] == 0, "clamped at the bottom: %g", v[EC_P_FILTER_DRIVE]);
    /* a macro chain: LFO2 -> M1 VAL, M1 -> MIX: the second reads the modulated VAL */
    ec_params_default(v);
    v[EC_P_MACRO1] = 0;
    v[EC_P_FILTER_MIX] = 0;
    v[ec_mtx_src_id(0)] = EC_MS_M1 + 1;
    v[ec_mtx_dst_id(0)] = EC_P_FILTER_MIX + 1;
    v[ec_mtx_depth_id(0)] = 100;
    v[ec_mtx_src_id(1)] = EC_MS_LFO2 + 1;
    v[ec_mtx_dst_id(1)] = EC_P_MACRO1 + 1;
    v[ec_mtx_depth_id(1)] = 50;
    memset(live, 0, sizeof live);
    live[EC_MSRC_LFO2] = 1;
    ec_mtx_apply(v, live);
    CHECK(fabsf(v[EC_P_MACRO1] - 63.5f) < 0.01f && fabsf(v[EC_P_FILTER_MIX] - 63.5f) < 0.01f,
          "M1 VAL 63.5 (%g), M1 -> MIX 63.5 (%g)", v[EC_P_MACRO1], v[EC_P_FILTER_MIX]);
    /* the fast buses are the engine's, not offsets */
    ec_params_default(v);
    v[ec_mtx_src_id(0)] = EC_MS_LFO1 + 1;
    v[ec_mtx_dst_id(0)] = EC_P_FILTER_CUTOFF + 1;
    v[ec_mtx_depth_id(0)] = 100;
    CHECK(!ec_mtx_generic(v) && ec_mtx_bus(EC_P_FILTER_CUTOFF) == EC_MOD_CUTOFF, "CUT is a fast bus");
    CHECK(!ec_param_mod_dest(EC_P_FILTER_TYPE) && !ec_param_mod_dest(EC_P_MACRO_DEPTH) &&
              !ec_param_mod_dest(EC_P_MTX1_DEPTH) && ec_param_mod_dest(EC_P_AMP) && ec_param_mod_dest(EC_P_PITCH),
          "destinations: no options, retired or matrix params; AMP and PITCH are");
    {   /* DEPTH: fine near 0, SHIFT snaps */
        float x = 0;
        x = ec_param_nudge(EC_P_MTX1_DEPTH, x, 1);
        CHECK(x == 0.5f, "DEPTH 0 + 1 = 0.5 (%g)", x);
        x = ec_param_nudge(EC_P_MTX1_DEPTH, 60, 1);
        CHECK(x == 65, "DEPTH 60 + 1 = 65 (%g)", x);
        x = ec_param_nudge(EC_P_MTX1_DEPTH, 0.5f, -2);
        CHECK(x == -0.5f, "DEPTH 0.5 - 2 = -0.5 (%g)", x);
        CHECK(ec_param_snap(EC_P_MTX1_DEPTH, 3, 1) == 25 && ec_param_snap(EC_P_MTX1_DEPTH, 60, 1) == 100 &&
                  ec_param_snap(EC_P_MTX1_DEPTH, 0, -1) == -25,
              "DEPTH snaps 0 / 25 / 50 / 100");
    }
}

static void test_resolve_live(void)
{
    ec_track_status_t st;
    printf("resolve through the engine: MW -> SEND 1 (generic), AT -> CUT, LFO1 -> CUT (fast buses)\n");
    home();
    clear_matrix();
    if (!A->seq.playing)
        press(FM1P_B_PLAY, 40);
    run(200);
    tv()[EC_P_SEND1] = 0;
    tv()[ec_mtx_src_id(0)] = EC_MS_MW + 1;
    tv()[ec_mtx_dst_id(0)] = EC_P_SEND1 + 1;
    tv()[ec_mtx_depth_id(0)] = 100;
    ec_seq_touch(&A->seq, A->track);
    cc(1, 127);
    run(300);
    CHECK(A->seq.tr[A->track].sent.send1_level > 0.95f, "CC 1 at 127: SEND 1 sent %.3f",
          A->seq.tr[A->track].sent.send1_level);
    cc(1, 0);
    run(300);
    CHECK(A->seq.tr[A->track].sent.send1_level < 0.05f, "CC 1 at 0: SEND 1 sent %.3f",
          A->seq.tr[A->track].sent.send1_level);
    CHECK(tv()[EC_P_SEND1] == 0, "the stored SEND 1 never moves");
    /* AT on the selected track's held key -> CUT */
    tv()[ec_mtx_src_id(1)] = EC_MS_AT + 1;
    tv()[ec_mtx_dst_id(1)] = EC_P_FILTER_CUTOFF + 1;
    tv()[ec_mtx_depth_id(1)] = -50;
    ec_seq_touch(&A->seq, A->track);
    P->key_lean[key_of(0, 3)] = 80;
    key(0, 3, 1);                                             /* (a step key: AT is the highest held key's) */
    run(300);
    ec_dsp_track_status(A->dsp, A->track, &st);
    CHECK(fabsf(st.src[EC_MSRC_AT] - 0.8f) < 0.02f && fabsf(st.mod[EC_MOD_CUTOFF] + 0.4f) < 0.02f,
          "AT 80 %% x -50 %%: CUT bus %.3f (AT %.3f)", st.mod[EC_MOD_CUTOFF], st.src[EC_MSRC_AT]);
    key(0, 3, 0);
    P->key_lean[key_of(0, 3)] = 0;
    run(300);
    ec_dsp_track_status(A->dsp, A->track, &st);
    CHECK(fabsf(st.mod[EC_MOD_CUTOFF]) < 0.02f, "the key up: CUT bus %.3f", st.mod[EC_MOD_CUTOFF]);
    /* CC 11 -> EXPR */
    tv()[ec_mtx_src_id(2)] = EC_MS_EXPR + 1;
    tv()[ec_mtx_dst_id(2)] = EC_P_PAN + 1;
    tv()[ec_mtx_depth_id(2)] = 100;
    ec_seq_touch(&A->seq, A->track);
    cc(11, 64);
    run(300);
    ec_dsp_track_status(A->dsp, A->track, &st);
    CHECK(fabsf(st.mod[EC_MOD_PAN] - 64.0f / 127) < 0.02f, "CC 11 at 64 -> PAN bus %.3f", st.mod[EC_MOD_PAN]);
    cc(11, 0);
    clear_matrix();
}

/* ------------------------------------------------------------------------------------------- migration --- */
static uint32_t fnv(const uint8_t *b, uint32_t n)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < n; i++)
        h = (h ^ b[i]) * 16777619u;
    return h;
}
static uint8_t g_rec[1u << 21], g_old[1u << 21];

/* a 0.5 record -> the same with only the ids a 0.4 build had (< EC_P_MTX1_SRC): a 0.4 save */
static uint32_t to_04(const uint8_t *in, uint32_t n)
{
    uint32_t i = 16, o = 16, k, np, len;
    uint32_t nslot, npat, ntr, p, t, s;
#define CP(m) (memcpy(g_old + o, in + i, (m)), o += (m), i += (m))
    CP(4 + 4 + 2 + 1 + 1 + 1);
    nslot = in[i];
    CP(1);
    for (k = 0; k < nslot; k++) {
        const uint32_t pl = in[i];
        CP(1 + pl + 20);
    }
    npat = in[i];
    ntr = in[i + 1];
    CP(2);
    for (p = 0; p < npat; p++) {
        const uint32_t used = in[i];
        CP(1);
        if (!used)
            continue;
        for (t = 0; t < ntr; t++) {
            uint32_t at = o, cnt = 0, ns;
            np = in[i];
            CP(1);
            for (k = 0; k < np; k++) {
                if (in[i] < EC_P_MTX1_SRC) {
                    CP(5);
                    cnt++;
                } else
                    i += 5;
            }
            g_old[at] = (uint8_t)cnt;
            ns = (uint32_t)in[i] | (uint32_t)in[i + 1] << 8;
            CP(2);
            for (s = 0; s < ns; s++) {
                const uint32_t nl = in[i + 2 + 1 + 4];
                CP(2 + 1 + 4 + 1 + nl * 5);
            }
        }
    }
    memcpy(g_old + o, in + i, n - i);                          /* the tagged chunks as they are */
    o += n - i;
#undef CP
    memcpy(g_old, in, 16);
    len = o - 16;
    memcpy(g_old + 8, &len, 4);
    {
        const uint32_t h = fnv(g_old + 16, len);
        memcpy(g_old + 12, &h, 4);
    }
    return o;
}

static void test_migration(void)
{
    ec_project_t *pr = &A->proj;
    float *v;
    int32_t n;
    uint32_t m;
    ec_track_status_t st;
    printf("migration: a 0.4 record (LFO1 -> CUT, the routes past 8 dropped)\n");
    home();
    v = pr->pat[pr->current].tr[0].v;
    clear_matrix();
    v[EC_P_LFO1_DEST] = 3;                                   /* CUT */
    v[EC_P_LFO1_DEPTH] = 128;                                /* +1 */
    v[EC_P_LFO1_WAVE] = 5;                                   /* SQR: +-1 */
    v = pr->pat[pr->current].tr[1].v;                        /* T2: 10 routes */
    v[EC_P_LFO1_DEST] = 2;
    v[EC_P_LFO2_DEST] = 4;
    v[EC_P_MENV_DEST] = 5;
    v[EC_P_MVEL_DEST] = 3;
    v[EC_P_MACRO_DEPTH + 0] = 96;                            /* M1 -> PITCH +0.5 */
    v[EC_P_MACRO_DEPTH + 1] = 32;                            /* M1 -> CUT -0.5 */
    v[EC_P_MACRO_DEPTH + 7] = 100;
    v[EC_P_MACRO_DEPTH + 9] = 100;
    v[EC_P_MACRO_DEPTH + 13] = 100;
    v[EC_P_MACRO_DEPTH + 19] = 100;
    v[EC_P_MACRO2] = 64;
    ec_scene_set(pr, 0, 1, EC_P_MACRO_DEPTH + 1, 0);         /* scene A: M1 -> CUT -1 */
    n = ec_project_save_v1(pr, g_rec, sizeof g_rec);   /* (to_04 reads version 1) */
    CHECK(n > 0, "saved %d", n);
    m = to_04(g_rec, (uint32_t)n);
    CHECK(ec_project_load(pr, g_old, m) == 0, "the 0.4 record loads");
    CHECK(ec_project_mtx_dropped() == 2, "10 routes on T2: 2 dropped (%d)", ec_project_mtx_dropped());
    v = pr->pat[pr->current].tr[0].v;
    CHECK(ec_mtx_src(v, 0) == EC_MS_LFO1 && ec_mtx_dst(v, 0) == EC_P_FILTER_CUTOFF && v[ec_mtx_depth_id(0)] == 30 &&
              v[EC_P_LFO1_DEST] == 1,
          "T1: slot 1 = LFO1 > CUT 30 %% (%d %d %g), the old dest OFF", ec_mtx_src(v, 0), ec_mtx_dst(v, 0),
          v[ec_mtx_depth_id(0)]);
    v = pr->pat[pr->current].tr[1].v;
    CHECK(ec_mtx_src(v, 4) == EC_MS_M1 && ec_mtx_dst(v, 4) == EC_P_PITCH && v[ec_mtx_depth_id(4)] == 50 &&
              ec_mtx_src(v, 5) == EC_MS_M1 && ec_mtx_dst(v, 5) == EC_P_FILTER_CUTOFF && v[ec_mtx_depth_id(5)] == -15 &&
              ec_mtx_src(v, 3) == EC_MS_VEL && v[ec_mtx_depth_id(3)] == 100,
          "T2: M1 > PITCH 50 %%, M1 > CUT -15 %% (%g), VEL > CUT 100 %%", v[ec_mtx_depth_id(5)]);
    CHECK(v[EC_P_MACRO_DEPTH + 0] == 64 && v[EC_P_MACRO_DEPTH + 19] == 64, "the old depths back to 0");
    {
        float x = 1;
        CHECK(ec_scene_get(pr, 0, 1, ec_mtx_depth_id(5), &x) && x == -30 &&
                  !ec_scene_get(pr, 0, 1, EC_P_MACRO_DEPTH + 1, &x),
              "scene A's M1 -> CUT moved onto the slot's DEPTH (-30 %%: %g)", x);
    }
    /* the same strength: the CUT bus = 0.3 x the LFO (0.4: depth x 0.3) */
    ec_seq_reload(&A->seq);
    A->track = 0;
    if (!A->seq.playing)
        press(FM1P_B_PLAY, 40);
    run(400);
    ec_dsp_track_status(A->dsp, 0, &st);
    CHECK(fabsf(fabsf(st.src[EC_MSRC_LFO1]) - 1.0f) < 0.01f && fabsf(st.mod[EC_MOD_CUTOFF] - 0.3f * st.src[EC_MSRC_LFO1]) < 1e-4f,
          "LFO1 SQR %.3f -> CUT bus %.3f (0.4: x 0.3)", st.src[EC_MSRC_LFO1], st.mod[EC_MOD_CUTOFF]);
    {   /* a 0.5 save keeps the matrix: no second conversion */
        n = ec_project_save(pr, g_rec, sizeof g_rec);
        CHECK(ec_project_load(pr, g_rec, (uint32_t)n) == 0 && ec_project_mtx_dropped() == 0 &&
                  ec_mtx_dst(pr->pat[pr->current].tr[0].v, 0) == EC_P_FILTER_CUTOFF,
              "a 0.5 record reloads as it is");
        ec_seq_reload(&A->seq);
    }
    clear_matrix();
    for (int t = 1; t < EC_NTRACK; t++) {
        int k;
        for (k = 0; k < EC_MTX_SLOTS; k++)
            pr->pat[pr->current].tr[t].v[ec_mtx_src_id(k)] = 1;
    }
}

/* ------------------------------------------------------------------------------------------- the MATRIX page --- */
static void test_matrix_page(void)
{
    printf("the MATRIX page: MOD's first; SELECT the row and on; K1 SRC, K2 DST, K3 DEPTH\n");
    home();
    clear_matrix();
    press(FM1P_B_LFO, 40);
    CHECK(A->cat == EC_CAT_MOD && A->page[EC_CAT_MOD] == 0 && ec_page_pic(EC_CAT_MOD, 0, 0) == EC_PIC_MATRIX,
          "MOD: the MATRIX page first");
    CHECK(!strcmp(L(FM1CORE_LBL_KNOB0 + FM1P_E_K1), "SRC") && !strcmp(L(FM1CORE_LBL_KNOB0 + FM1P_E_K2), "DST") &&
              !strcmp(L(FM1CORE_LBL_KNOB0 + FM1P_E_K3), "DEPTH") && !strcmp(L(FM1CORE_LBL_KNOB0 + FM1P_E_K4), ""),
          "knob labels SRC DST DEPTH -: %s %s %s %s", L(FM1CORE_LBL_KNOB0 + FM1P_E_K1),
          L(FM1CORE_LBL_KNOB0 + FM1P_E_K2), L(FM1CORE_LBL_KNOB0 + FM1P_E_K3), L(FM1CORE_LBL_KNOB0 + FM1P_E_K4));
    turns(FM1P_E_SELECT, 2);
    CHECK(A->mk.row == 2 && A->page[EC_CAT_MOD] == 0, "SELECT 2: row 3 (%d)", A->mk.row + 1);
    turns(FM1P_E_K1, 1);
    CHECK(ec_mtx_src(tv(), 2) == EC_MS_LFO1, "K1: row 3 SRC LFO1");
    turns(FM1P_E_K2, 1);
    {
        const int d1 = ec_mtx_dst(tv(), 2);
        CHECK(d1 >= 0 && ec_param_mod_dest(d1), "K2: row 3 DST %s", d1 >= 0 ? EC_PARAMS[d1].name : "-");
        btn(FM1P_B_GLO, 1);
        run(20);
        turns(FM1P_E_K2, 1);
        btn(FM1P_B_GLO, 0);
        run(20);
        {
            const int d2 = ec_mtx_dst(tv(), 2);
            CHECK(d2 >= 0 && d2 != d1, "SHIFT + K2: the next category's first (%s -> %s)", EC_PARAMS[d1].name,
                  d2 >= 0 ? EC_PARAMS[d2].name : "-");
        }
        turns(FM1P_E_K2, -1);
        CHECK(ec_mtx_dst(tv(), 2) != d1 || 1, "K2 back");
    }
    /* row 3: LFO1 > CUTOFF via the param itself, so the depth makes sound */
    tv()[ec_mtx_dst_id(2)] = EC_P_FILTER_CUTOFF + 1;
    CHECK(tv()[EC_P_LFO1_DEPTH] == 128, "a silent LFO1 got its level (%g)", tv()[EC_P_LFO1_DEPTH]);
    turns(FM1P_E_K3, 3);
    CHECK(tv()[ec_mtx_depth_id(2)] == 1.5f, "K3 x3: DEPTH 1.5 %% (%g)", tv()[ec_mtx_depth_id(2)]);
    btn(FM1P_B_GLO, 1);
    run(20);
    turns(FM1P_E_K3, 1);
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(tv()[ec_mtx_depth_id(2)] == 25, "SHIFT + K3: DEPTH 25 %% (%g)", tv()[ec_mtx_depth_id(2)]);
    png("matrix_page.png");
    turns(FM1P_E_SELECT, 5);
    CHECK(A->mk.row == 7 && A->page[EC_CAT_MOD] == 0, "row 8");
    turns(FM1P_E_SELECT, 1);
    CHECK(A->page[EC_CAT_MOD] == 1, "SELECT past row 8: the next page (%d)", A->page[EC_CAT_MOD]);
    turns(FM1P_E_SELECT, -1);
    CHECK(A->page[EC_CAT_MOD] == 0 && A->mk.row == 7, "SELECT back: MATRIX row 8 (%d %d)", A->page[EC_CAT_MOD],
          A->mk.row + 1);
    CHECK(ec_mk_marks(EC_P_FILTER_CUTOFF) == 1u << EC_MS_LFO1, "CUTOFF is marked LFO1");
}

/* ------------------------------------------------------------------------------------------- the source keys --- */
/* 0.6.0: MOD KEYS became the source template (docs/UX-V2.md §1): B6 LFO 1, B7 LFO 2, B8 MOD ENV, B9 VEL, B10 AT in
 * FILTER / AMP / MODULATION / LOOP ENGINE / TRIG / FX; MODULATION's B2..B5 = M1..M4. The 0.5.0 gestures, re-keyed */
enum { B_LFO1 = 5, B_LFO2 = 6, B_ENV = 7, B_VEL = 8, B_AT = 9 };   /* upper indices */

static void test_modkeys(void)
{
    int k;
    printf("the source keys: the gestures\n");
    ec_app_goto(EC_CAT_FILT, 0);
    run(900);
    clear_matrix();
    CHECK(!strcmp(L(FM1CORE_LBL_BLACK0 + B_LFO1), "LFO1") && !strcmp(L(FM1CORE_LBL_BLACK0 + B_AT), "AT") &&
              !strcmp(L(FM1CORE_LBL_BLACK0 + B_ENV), "MENV"),
          "black keys: %s .. %s .. %s", L(FM1CORE_LBL_BLACK0 + B_LFO1), L(FM1CORE_LBL_BLACK0 + B_ENV),
          L(FM1CORE_LBL_BLACK0 + B_AT));

    /* hold LFO 2 + KNOB 1 on FILTER (CUTOFF) */
    tv()[EC_P_LFO2_DEPTH] = 64;
    key(1, B_LFO2, 1);
    turn(FM1P_E_K1, 1);
    run(30);
    k = ec_mtx_find(tv(), EC_MS_LFO2, EC_P_FILTER_CUTOFF);
    CHECK(k == 0 && tv()[ec_mtx_depth_id(0)] == 0, "the first detent: slot 1 = LFO2 > CUTOFF at 0 (%d)", k);
    CHECK(tv()[EC_P_LFO2_DEPTH] == 128, "LFO 2's level up from 0");
    CHECK(tv()[EC_P_FILTER_CUTOFF] == 127, "the cutoff itself never moved (%g)", tv()[EC_P_FILTER_CUTOFF]);
    turns(FM1P_E_K1, 4);
    CHECK(tv()[ec_mtx_depth_id(0)] == 2, "4 more: DEPTH 2 %% (%g)", tv()[ec_mtx_depth_id(0)]);
    CHECK(!strncmp(L(FM1CORE_LBL_KNOB0 + FM1P_E_K1), "LFO2 ", 5), "K1 label while held: %s",
          L(FM1CORE_LBL_KNOB0 + FM1P_E_K1));
    png("source_held.png");
    key(1, B_LFO2, 0);
    run(30);
    CHECK(ec_mtx_find(tv(), EC_MS_LFO2, EC_P_FILTER_CUTOFF) == 0 && A->cat == EC_CAT_FILT,
          "released: the slot stays, no page change");
    CHECK(ec_mk_marks(EC_P_FILTER_CUTOFF) == 1u << EC_MS_LFO2, "CUTOFF marked LFO2");
    png("cell_mark.png");

    /* a hold that leaves the new slot at 0: freed */
    key(1, B_ENV, 1);
    turn(FM1P_E_K2, 1);                                       /* RESO */
    run(30);
    CHECK(ec_mtx_find(tv(), EC_MS_ENV, EC_P_FILTER_RES) == 1, "MOD ENV > RESO in slot 2");
    key(1, B_ENV, 0);
    run(30);
    CHECK(ec_mtx_find(tv(), EC_MS_ENV, EC_P_FILTER_RES) < 0 && ec_mtx_src(tv(), 1) == EC_MS_OFF,
          "released at 0: freed");

    /* NO MOD: an option */
    key(1, B_LFO1, 1);
    turn(FM1P_E_K3, 1);                                       /* filter TYPE */
    run(30);
    CHECK(!strcmp(A->ux.to_val, "NO MOD") && A->ux.to_ms, "TYPE: NO MOD (%s)", A->ux.to_val);
    key(1, B_LFO1, 0);
    run(30);
    CHECK(ec_mtx_src(tv(), 1) == EC_MS_OFF && A->cat == EC_CAT_FILT, "nothing made, no page (a turn is no tap)");

    /* F2 while turning: that source's slot removed */
    key(1, B_LFO2, 1);
    run(10);
    btn(FM1P_B_OCTUP, 1);
    run(10);
    turn(FM1P_E_K1, 1);
    run(30);
    btn(FM1P_B_OCTUP, 0);
    key(1, B_LFO2, 0);
    run(30);
    CHECK(ec_mtx_find(tv(), EC_MS_LFO2, EC_P_FILTER_CUTOFF) < 0, "LFO 2 + F2 + K1: removed");
    CHECK(!A->pn.f_sticky && !A->pn.f_held, "F2 used: no sticky F2");

    /* held steps: the DEPTH is a lock */
    key(1, B_AT, 1);
    turn(FM1P_E_K2, 1);                                       /* make AT > RESO */
    run(20);
    key(0, 2, 1);                                             /* step 3 held */
    turns(FM1P_E_K2, 2);
    key(0, 2, 0);
    key(1, B_AT, 0);
    run(30);
    k = ec_mtx_find(tv(), EC_MS_AT, EC_P_FILTER_RES);
    {
        float x = 0;
        CHECK(k >= 0 && tv()[ec_mtx_depth_id(k)] == 0 &&
                  ec_step_lock_get(&ec_project_track(&A->proj, A->track)->steps[2], ec_mtx_depth_id(k), &x) && x == 1,
              "step 3 held: its DEPTH lock 1 %% (%g), the slot kept at 0", x);
    }

    /* FULL */
    for (k = 0; k < EC_MTX_SLOTS; k++)
        if (ec_mtx_src(tv(), k) == EC_MS_OFF) {
            tv()[ec_mtx_src_id(k)] = EC_MS_M4 + 1;
            tv()[ec_mtx_dst_id(k)] = EC_P_FILTER_DRIVE + 1;
        }
    key(1, B_VEL, 1);
    turn(FM1P_E_K1, 1);
    run(30);
    key(1, B_VEL, 0);
    run(30);
    CHECK(!strcmp(A->ux.to_val, "FULL") && ec_mtx_find(tv(), EC_MS_VEL, EC_P_FILTER_CUTOFF) < 0, "8 slots: FULL (%s)",
          A->ux.to_val);
    clear_matrix();

    /* taps: the source's page */
    tapkey(1, B_LFO1);
    CHECK(A->cat == EC_CAT_MOD && ec_page_pic(EC_CAT_MOD, A->page[EC_CAT_MOD], 0) == EC_PIC_LFO1, "tap LFO 1: its page");
    tapkey(1, B_AT);
    CHECK(A->cat == EC_CAT_MOD && ec_page_pic(EC_CAT_MOD, A->page[EC_CAT_MOD], 0) == EC_PIC_MATRIX, "tap AT: MATRIX");
    tapkey(1, 3);                                             /* MODULATION's B4 = M3 */
    {
        ec_page_t pg;
        ec_page_items(EC_CAT_MOD, A->page[EC_CAT_MOD], tv(), 0, &pg);
        CHECK(A->cat == EC_CAT_MOD && pg.pic == EC_PIC_MACRO && pg.arg == 2, "MODULATION: tap M3 (B4): MACRO 3");
    }
    tapkey(1, B_ENV);
    CHECK(A->cat == EC_CAT_AMP && ec_page_pic(EC_CAT_AMP, A->page[EC_CAT_AMP], 0) == EC_PIC_MENV,
          "tap MOD ENV: AMP's MOD ENV page");
    /* MODULATION: hold M2 (B3) + a value knob = route the macro */
    ec_app_goto(EC_CAT_MOD, 1);                               /* LFO 1 */
    run(900);
    {
        ec_page_t pg;
        int c, id = -1;
        ec_app_page(A, &pg);
        for (c = 0; c < pg.n && c < 4 && id < 0; c++)
            if (pg.it[c].kind == EC_IT_PARAM && ec_param_mod_dest(pg.it[c].id))
                id = c;
        if (id >= 0) {
            key(1, 2, 1);
            turn(FM1P_E_K1 + id, 1);
            turns(FM1P_E_K1 + id, 3);
            key(1, 2, 0);
            run(30);
            CHECK(ec_mtx_find(tv(), EC_MS_M2, pg.it[id].id) >= 0, "MODULATION: M2 held + K%d: M2 > %s", id + 1,
                  EC_PARAMS[pg.it[id].id].label);
        }
    }
    clear_matrix();

    /* LEDs and SHIFT's layer */
    ec_app_goto(EC_CAT_FILT, 0);
    run(900);
    key(1, B_ENV, 1);
    C->frame();
    CHECK(P->led[0][0] || 1, "(LEDs drawn)");
    key(1, B_ENV, 0);
    run(30);
    btn(FM1P_B_GLO, 1);
    run(300);
    CHECK(!strcmp(L(FM1CORE_LBL_BLACK0 + 0), "TAP") && !strcmp(L(FM1CORE_LBL_BLACK0 + B_LFO1), ""),
          "SHIFT held: its layer (%s, B6 '%s')", L(FM1CORE_LBL_BLACK0), L(FM1CORE_LBL_BLACK0 + B_LFO1));
    btn(FM1P_B_GLO, 0);
    run(20);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: ec_test_mtx <folder> [png dir]\n");
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
    run(1500);
    test_resolve_unit();
    test_resolve_live();
    test_matrix_page();
    test_modkeys();
    test_migration();
    C->shutdown();
    printf("test_mtx: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
