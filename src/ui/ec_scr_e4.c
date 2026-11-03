/* SPDX-License-Identifier: GPL-3.0-only */
/* E4 screens: the picture above the cells on the FX pages (0.6.0: the track's FX chain, the approved mock's slot list),
 * SENDS (levels + the tap point), and MOD (LFO shapes with the live value, the mod envelope, velocity, the macros and
 * their matrix). On the 120x120 grid in palette slots (ec_gfx.h). */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_gfx.h"
#include "ec_mtx.h"
#include "ec_scr_e4.h"

#define W EC_GW
#define R(c, x, y, w, h) ec_gfx_rect((x), (y), (w), (h), (c))
#define T(c, s, x, y, al) ec_gfx_text_al((x), (y), (s), 1, (c), (al))
#define T2(c, s, x, y, al) ec_gfx_text_al((x), (y), (s), 2, (c), (al))
#define LINE(c, x0, y0, x1, y1) ec_gfx_line((int)floorf((x0) + 0.5f), (int)floorf((y0) + 0.5f), \
                                            (int)floorf((x1) + 0.5f), (int)floorf((y1) + 0.5f), (c))

static float clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }

/* a param as the page shows it (the bus record, a held step's lock, else the track's) */
static float val(const ec_app_t *a, int id)
{
    ec_item_t it;
    it.kind = EC_IT_PARAM;
    it.id = (uint8_t)id;
    it.flags = 0;
    it.label = 0;
    return ec_app_item_value(a, &it, 0);
}
static float amt(const ec_app_t *a, int id) { return clampf(val(a, id) / 127.0f, 0, 1); }
static float bip(const ec_app_t *a, int id) { return clampf((val(a, id) - 64.0f) / 64.0f, -1, 1); }
static int opt(const ec_app_t *a, int id) { return (int)floorf(val(a, id) + 0.5f); }

/* a polyline of n points y[i] (grid units) across x0..x1 */
static void trace(const float *y, int n, float x0, float x1, int c)
{
    int i;
    for (i = 0; i + 1 < n; i++) {
        const float xa = x0 + (x1 - x0) * i / (n - 1), xb = x0 + (x1 - x0) * (i + 1) / (n - 1);
        LINE(c, xa, y[i], xb, y[i + 1]);
    }
}

/* ------------------------------------------------------------------------------------------- the visuals' area --- */
#define VX0 3
#define VX1 108
#define VT 34
#define VB 70

/* FX (0.6.0, the approved mock): the four FX slots of the selected track's chain, one a row: the slot (INSERT, SEND 1,
 * SEND 2, MASTER; there is one insert per track), its engine big, its mix as a bar (the insert's FX MIX, the track's
 * send levels, the master bus's FX MIX); the slot the page edits highlighted. An engine swap still fading: an accent
 * notch under its name. */
static void fx_page(const ec_app_t *a, const ec_page_t *pg)
{
    static const char *const SLOT[4] = {"INSERT", "SEND 1", "SEND 2", "MASTER"};
    const float *tv = a->proj.pat[a->proj.current].tr[a->track].v;
    int i;
    for (i = 0; i < 4; i++) {
        const int y = 12 + i * 15, on = i == pg->busv;
        const int m = (int)floorf((i ? a->proj.bus[i - 1][EC_P_FX_MACHINE] : tv[EC_P_FX_MACHINE]) + 0.5f);
        const float mix = i == 0 ? tv[EC_P_FX_MIX] : i < 3 ? tv[i == 1 ? EC_P_SEND1 : EC_P_SEND2]
                                                           : a->proj.bus[2][EC_P_FX_MIX];
        int running = m;
        if (on)
            R(EC_C_L3, 0, y, W, 13);
        T(on ? EC_C_L15 : EC_C_L8, SLOT[i], 3, y + 4, EC_AL_LEFT);
        T2(m == EC_FX_NONE ? EC_C_L5 : on ? EC_C_L15 : EC_C_L12, m == EC_FX_NONE ? "--" : ec_fx_short_name(m), 34,
           y + 2, EC_AL_LEFT);
        if (on) {
            if (pg->busv) {
                ec_bus_status_t bs;
                ec_dsp_bus_status(a->dsp, pg->busv - 1, &bs);
                running = bs.fx_running;
            } else {
                ec_track_status_t st;
                ec_dsp_track_status(a->dsp, a->track, &st);
                running = st.fx_running;
            }
        }
        if (running != m)                          /* the swap fade in progress */
            R(EC_C_ACC, 34, y + 12, 6, 1);
        R(EC_C_L5, 100, y + 6, 17, 1);
        R(on ? EC_C_ACC : EC_C_L12, 100, y + 5, (int)(17.0f * clampf(mix / 127.0f, 0, 1) + 0.5f), 3);
    }
}

static void sends_page(const ec_app_t *a)
{
    const float s1 = amt(a, EC_P_SEND1), s2 = amt(a, EC_P_SEND2);
    const int post = opt(a, EC_P_SEND_TAP) == 2;
    const int y = 24;
    int i;
    /* the chain: FILT -> FX -> MIX, the tap before or after the insert */
    R(EC_C_L5, 4, y, 22, 9), T(EC_C_BG, "FILT", 15, y + 2, EC_AL_CENTER);
    R(EC_C_L12, 46, y, 22, 9), T(EC_C_BG, "FX", 57, y + 2, EC_AL_CENTER);
    R(EC_C_L5, 88, y, 22, 9), T(EC_C_BG, "MIX", 99, y + 2, EC_AL_CENTER);
    R(EC_C_L8, 26, y + 4, 20, 1), R(EC_C_L8, 68, y + 4, 20, 1);
    {
        const int tx = post ? 78 : 36;
        R(EC_C_ACC, tx, y + 4, 1, 14);
        R(EC_C_ACC, tx - 2, y + 4, 5, 2);
    }
    for (i = 0; i < 2; i++) {
        const float lv = i ? s2 : s1;
        const int by = 44 + i * 13;
        T(EC_C_L8, i ? "S2" : "S1", 4, by, EC_AL_LEFT);
        R(EC_C_L3, 16, by, 92, 5);
        R(lv > 0 ? EC_C_L12 : EC_C_L3, 16, by, (int)(lv * 92), 5);
        T(EC_C_L5, ec_fx_short_name((int)(a->proj.bus[i][EC_P_FX_MACHINE] + 0.5f)), 108, by + 7, EC_AL_RIGHT);
    }
}

/* ------------------------------------------------------------------------------------------- MOD --- */
static float wave_at(int w, float ph)
{
    switch (w) {
    case 2: return 1 - fabsf(ph * 2 - 1) * 2;
    case 3: return ph * 2 - 1;
    case 4: return 1 - ph * 2;
    case 5: return ph < 0.5f ? 1.f : -1.f;
    case 6: {                                   /* S&H: a fixed pattern */
        static const float S[8] = {0.6f, -0.3f, 0.9f, -0.8f, 0.1f, -0.5f, 0.75f, -0.1f};
        return S[(int)(ph * 8) & 7];
    }
    default: return sinf(ph * 6.2831853f);
    }
}

/* 0.5.0: the source's routes in the matrix ("> CUT +30%", more: "+2"), dim "> OFF" when it has none, and its live
 * value as a bipolar meter at the right */
static void routes(const ec_app_t *a, int src, int small)
{
    const float *v = a->proj.pat[a->proj.current].tr[a->track].v;
    char b[32], nm[16], dp[12];
    int k, n = 0, first = -1;
    for (k = 0; k < EC_MTX_SLOTS; k++)
        if (ec_mtx_src(v, k) == src && ec_mtx_dst(v, k) >= 0) {
            if (first < 0)
                first = k;
            n++;
        }
    if (first < 0)
        snprintf(b, sizeof b, "> OFF");
    else {
        ec_mk_dst_name(v[ec_mtx_dst_id(first)], nm, sizeof nm);
        ec_param_format(ec_mtx_depth_id(first), v[ec_mtx_depth_id(first)], 3, dp, sizeof dp);
        if (n > 1)
            snprintf(b, sizeof b, "> %s %s +%d", nm, dp, n - 1);
        else
            snprintf(b, sizeof b, "> %s %s", nm, dp);
    }
    if (small) {                                /* over the envelope bars: no meter */
        T(first < 0 ? EC_C_L5 : EC_C_L15, b, 2, 13, EC_AL_LEFT);
        return;
    }
    T2(first < 0 ? EC_C_L5 : EC_C_L15, b, 2, 15, EC_AL_LEFT);
    {
        ec_track_status_t st;
        float x;
        ec_dsp_track_status(a->dsp, a->track, &st);
        x = src >= EC_MS_LFO1 && src <= EC_MS_EXPR ? st.src[src - EC_MS_LFO1] : 0.0f;
        {   /* a bipolar meter at the right */
            const int mx = W - 6, top = VT - 8, bot = VB, mid = (top + bot) / 2;
            const int h = (int)(clampf(x, -1, 1) * (bot - top) / 2);
            R(EC_C_L3, mx, top, 2, bot - top);
            if (h > 0)
                R(EC_C_ACC, mx, mid - h, 2, h);
            else
                R(EC_C_ACC, mx, mid, 2, -h + 1);
            R(EC_C_L8, mx - 1, mid, 4, 1);
        }
    }
}

static void lfo_page(const ec_app_t *a, int k)
{
    const int wave = opt(a, k ? EC_P_LFO2_WAVE : EC_P_LFO1_WAVE);
    const int mode = opt(a, k ? EC_P_LFO2_MODE : EC_P_LFO1_MODE);
    const float d = bip(a, k ? EC_P_LFO2_DEPTH : EC_P_LFO1_DEPTH);
    const int spd = opt(a, k ? EC_P_LFO2_SPEED : EC_P_LFO1_SPEED);
    const float cyc = mode == 3 ? 1.0f : clampf(powf(2.0f, (spd - 4) * 0.5f) * 2, 1, 6);
    float y[104];
    const float mid = (VT - 6 + VB) / 2.0f, hh = (VB - VT + 6) / 2.0f;
    int i;
    routes(a, k ? EC_MS_LFO2 : EC_MS_LFO1, 0);
    for (i = 0; i < 104; i++) {
        const float x = (float)i / 103 * cyc, ph = x - floorf(x);
        const float w = mode == 3 && x >= 1 ? wave_at(wave, 1.0f) : wave_at(wave, mode == 3 ? x : ph);
        y[i] = mid - w * d * hh;
    }
    ec_gfx_dots_h(VX0, VX1, (int)mid, 2, EC_C_L3);
    if (mode == 2 || mode == 3)                 /* TRIG / ONE: the reset at the note */
        R(EC_C_L8, VX0, VT - 6, 1, VB - VT + 6);
    trace(y, 104, VX0, VX1, d == 0 ? EC_C_L5 : EC_C_L12);
}

/* MOD ENV: the norns envelope bars are drawn by ec_screens.c (y 20..75); here the source's routes, small, above them */
static void menv_page(const ec_app_t *a) { routes(a, EC_MS_ENV, 1); }

static void vel_page(const ec_app_t *a)
{
    const float d = bip(a, EC_P_MVEL_DEPTH), mid = (VT + VB) / 2.0f, hh = (VB - VT) / 2.0f;
    int k;
    routes(a, EC_MS_VEL, 0);
    for (k = 0; k < 8; k++) {                   /* velocity 1/8 .. 1 -> the push */
        const float v = (k + 1) / 8.0f * d;
        const int x = VX0 + 4 + k * 12, h = (int)(fabsf(v) * hh);
        R(EC_C_L3, x, VT, 8, VB - VT);
        R(EC_C_L12, x, v >= 0 ? (int)mid - h : (int)mid, 8, h > 0 ? h : 1);
    }
}

static void macros_page(const ec_app_t *a, int single)
{
    ec_track_status_t st;
    int k;
    ec_dsp_track_status(a->dsp, a->track, &st);
    if (single < 0) {
        for (k = 0; k < 4; k++) {
            const float v = amt(a, EC_P_MACRO1 + k);
            const int x = 8 + k * 26, h = (int)(v * (VB - 16));
            char b[4];
            R(EC_C_L3, x, 16, 14, VB - 16);
            R(EC_C_L12, x, VB - h, 14, h);
            R(EC_C_ACC, x - 2, VB - (int)(st.macro[k] * (VB - 16)), 18, 1);
            snprintf(b, sizeof b, "M%d", k + 1);
            T(EC_C_L8, b, x + 7, VB + 2, EC_AL_CENTER);
        }
        return;
    }
    {   /* one macro: its value, and its rows of the matrix (read-only): DST, DEPTH from the centre */
        const float *tvv = a->proj.pat[a->proj.current].tr[a->track].v;
        const float v = amt(a, EC_P_MACRO1 + single);
        int y = 16, n = 0;
        R(EC_C_L3, 4, 16, 10, VB - 16);
        R(EC_C_L15, 4, VB - (int)(v * (VB - 16)), 10, (int)(v * (VB - 16)));
        R(EC_C_ACC, 2, VB - (int)(st.macro[single] * (VB - 16)), 14, 1);
        for (k = 0; k < EC_MTX_SLOTS && n < 6; k++) {
            char nm[16], dp[12];
            float d;
            int w;
            if (ec_mtx_src(tvv, k) != EC_MS_M1 + single || ec_mtx_dst(tvv, k) < 0)
                continue;
            d = tvv[ec_mtx_depth_id(k)];
            ec_mk_dst_name(tvv[ec_mtx_dst_id(k)], nm, sizeof nm);
            ec_param_format(ec_mtx_depth_id(k), d, 3, dp, sizeof dp);
            T(EC_C_L12, nm, 20, y + 1, EC_AL_LEFT);
            T(EC_C_L8, dp, 76, y + 1, EC_AL_RIGHT);
            R(EC_C_L3, 80, y + 3, 36, 1);
            w = (int)(fabsf(d) / 100.0f * 18.0f + 0.5f);
            R(EC_C_L15, d >= 0 ? 98 : 98 - w, y + 2, w > 0 ? w : 1, 3);
            R(EC_C_L8, 98, y + 1, 1, 5);
            y += 9;
            n++;
        }
        if (!n)
            T(EC_C_L5, "> OFF", 20, 17, EC_AL_LEFT);
    }
}

/* 0.5.0: the MATRIX page: the eight slots, SRC DST DEPTH (a bar from the centre, the live amount as an orange tick);
 * the selected row bright cream, the others grey */
static void matrix_page(const ec_app_t *a)
{
    const float *v = a->proj.pat[a->proj.current].tr[a->track].v;
    ec_track_status_t st;
    int k;
    ec_dsp_track_status(a->dsp, a->track, &st);
    for (k = 0; k < EC_MTX_SLOTS; k++) {     /* the mock's MATRIX rows: 8 units apart from y 12 */
        const int y = 12 + k * 8, sel = k == a->mk.row, src = ec_mtx_src(v, k), live = ec_mtx_live(v, k);
        const int used = src && ec_mtx_dst(v, k) >= 0;
        const float d = v[ec_mtx_depth_id(k)];
        const int c = sel ? EC_C_L15 : used ? EC_C_L12 : EC_C_L5;
        char n[4], nm[16], dp[12];
        int w;
        if (sel)
            R(EC_C_L3, 0, y, W, 8);
        snprintf(n, sizeof n, "%d", k + 1);
        T(sel ? EC_C_L15 : EC_C_L5, n, 2, y + 2, EC_AL_LEFT);
        T(c, src ? ec_mtx_src_name(src) : "--", 9, y + 2, EC_AL_LEFT);
        ec_mk_dst_name(v[ec_mtx_dst_id(k)], nm, sizeof nm);
        T(c, ec_mtx_dst(v, k) >= 0 ? nm : "--", 29, y + 2, EC_AL_LEFT);
        ec_param_format(ec_mtx_depth_id(k), d, 3, dp, sizeof dp);
        T(c, dp, 78, y + 2, EC_AL_RIGHT);
        R(EC_C_L5, 84, y + 4, 34, 1);
        w = (int)(fabsf(d) / 100.0f * 16.0f + 0.5f);
        if (w)
            R(sel ? EC_C_L15 : used ? EC_C_L12 : EC_C_L5, d >= 0 ? 101 : 101 - w, y + 3, w, 3);
        R(EC_C_L12, 101, y + 2, 1, 5);
        if (live) {                                 /* the amount now: source x depth */
            float x = 0;
            if (src >= EC_MS_LFO1 && src <= EC_MS_EXPR)
                x = st.src[src - EC_MS_LFO1];
            else if (src >= EC_MS_M1)
                x = st.macro[src - EC_MS_M1];
            x = clampf(x * d / 100.0f, -1, 1);
            R(EC_C_ACC, 101 + (int)(x * 16.0f), y + 2, 1, 5);
        }
    }
}

void ec_scr_e4(const ec_app_t *a, const ec_page_t *pg)
{
    switch (pg->pic) {
    case EC_PIC_FX: fx_page(a, pg); break;
    case EC_PIC_SENDS: sends_page(a); break;
    case EC_PIC_LFO1: lfo_page(a, 0); break;
    case EC_PIC_LFO2: lfo_page(a, 1); break;
    case EC_PIC_MENV: menv_page(a); break;
    case EC_PIC_VEL: vel_page(a); break;
    case EC_PIC_MACROS: macros_page(a, -1); break;
    case EC_PIC_MACRO: macros_page(a, pg->arg); break;
    case EC_PIC_MATRIX: matrix_page(a); break;   /* 0.5.0 */
    default: break;
    }
}
