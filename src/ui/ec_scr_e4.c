/* SPDX-License-Identifier: GPL-3.0-only */
/* E4 screens: the picture above the cells on the FX pages (0.6.0: the track's FX chain, the approved mock's slot list),
 * SENDS (levels + the tap point), and MOD (LFO shapes with the live value, the mod envelope, velocity, the macros and
 * their matrix). On the 120x120 grid in palette slots (ec_gfx.h). */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_gfx.h"
#include "ec_holdmenu.h"
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

/* 0.6.3 (docs/UX-V2.md §10 "FX layout"): a slot's level (K4 MIX; the sends' rows: the track's send levels) */
static float slot_level(const ec_app_t *a, int slot)
{
    const float *tv = a->proj.pat[a->proj.current].tr[a->track].v;
    switch (slot) {
    case 0: return tv[ec_fx_mix_id((int)floorf(tv[EC_P_FX_MACHINE] + 0.5f))];
    case 1: return tv[EC_P_INS2_MIX];
    case 2: return tv[EC_P_SEND1];
    case 3: return tv[EC_P_SEND2];
    default: {
        const float *bv = a->proj.bus[2];
        return bv[ec_fx_mix_id((int)floorf(bv[EC_P_FX_MACHINE] + 0.5f))];
    }
    }
}
static int slot_mach(const ec_app_t *a, int slot)
{
    const float *tv = a->proj.pat[a->proj.current].tr[a->track].v;
    const float x = slot >= 2 ? a->proj.bus[slot - 2][EC_P_FX_MACHINE] : tv[slot ? EC_P_INS2_MACHINE : EC_P_FX_MACHINE];
    return (int)floorf(x + 0.5f);
}
static int slot_running(const ec_app_t *a, int slot)   /* the engine running (a swap still fading: not the machine) */
{
    if (slot >= 2) {
        ec_bus_status_t bs;
        ec_dsp_bus_status(a->dsp, slot - 2, &bs);
        return bs.fx_running;
    } else {
        ec_track_status_t st;
        ec_dsp_track_status(a->dsp, a->track, &st);
        return slot ? st.fx2_running : st.fx_running;
    }
}

/* one slot's row: its name small, its engine big, its level as a bar, its knob pages as boxes under the bar (the
 * one shown lit on the selected row); selected: an L3 band. An engine swap still fading: an accent block */
static void slot_row(const ec_app_t *a, int slot, int y, int on)
{
    const int m = slot_mach(a, slot);
    const int np = a == &ec_app ? ec_src_ins_pages_of(slot) : 1, kp = on ? a->pn.ins_page : -1;
    const float lv = slot_level(a, slot);
    int k;
    if (on)
        R(EC_C_L3, 0, y, W, 14);
    T(on ? EC_C_L15 : EC_C_L8, ec_fx_slot_name(slot), 3, y + 5, EC_AL_LEFT);
    T2(m == EC_FX_NONE ? EC_C_L5 : on ? EC_C_L15 : EC_C_L12, m == EC_FX_NONE ? "--" : ec_fx_short_name(m), 40, y + 2,
       EC_AL_LEFT);
    if (slot_running(a, slot) != m)
        R(EC_C_ACC, 40, y + 12, 6, 2);
    R(on ? EC_C_L5 : EC_C_L3, 98, y + 4, 20, 4);
    R(on ? EC_C_ACC : EC_C_L12, 98, y + 4, (int)(10.0f * clampf(lv / 127.0f, 0, 1) + 0.5f) * 2, 4);
    for (k = 0; k < np && k < 3; k++)              /* the knob pages: P1 P2 .. */
        R(k == kp ? EC_C_L15 : on ? EC_C_L8 : EC_C_L5, 98 + k * 6, y + 10, 4, 2);
}

/* EFFECTS: INSERT 1, INSERT 2, SEND 1, SEND 2 a row each, the slot shown highlighted */
static void effects_page(const ec_app_t *a, int sel)
{
    int i;
    for (i = 0; i < 4; i++)
        slot_row(a, i, 12 + i * 14, i == sel);
}

/* MASTER: the master bus's effect (its row, its knob pages) and the load meter: the counted engine load (ec_dsp_load:
 * 1.0 = the hosts' 2x real time budget) as a chunky bar. No warning colour: the bar is the bar */
static void master_page(const ec_app_t *a)
{
    char b[16];
    const float ld = clampf(ec_src_load(), 0, 1);
    slot_row(a, 4, 12, 1);
    T(EC_C_L8, "LOAD", 3, 41, EC_AL_LEFT);
    R(EC_C_L3, 22, 38, 76, 10);
    R(EC_C_L12, 22, 38, (int)(ld * 38.0f + 0.5f) * 2, 10);
    snprintf(b, sizeof b, "%d%%", (int)floorf(ec_src_load() * 100.0f + 0.5f));
    T(EC_C_L15, b, 117, 41, EC_AL_RIGHT);
}

/* SENDS (§10): the track's routing, FILT -> I 1 -> I 2 -> AMP -> MIX, the send tap (ACC) where the send mode puts it
 * (PRE: after the filter; POST: after AMP), down to S 1 / S 2 and on to MST; then K1-K4's levels: I1 / I2 (the
 * inserts' MIX), S1 / S2 (the sends), each with its effect's name. 0.6.4: the boxes say only I 1, I 2, S 1, S 2; a
 * held B1-B4 raises its box (lifted 2 units, bright, a shadow under it) and K1 moves it left / right */
static void block(int x, int w, int y, int c, const char *s)
{
    R(c, x, y, w, 10);
    T(EC_C_BG, s, x + w / 2, y + 3, EC_AL_CENTER);
}
static void slot_block(int x, int w, int y, int c, const char *s, int raised)
{
    if (!raised) {
        block(x, w, y, c, s);
        return;
    }
    R(EC_C_L3, x + 2, y, w, 10);                   /* the shadow where it sat */
    block(x, w, y - 2, EC_C_L15, s);
}
static const char *mach_name(const ec_app_t *a, int slot)
{
    const int m = slot_mach(a, slot);
    return m == EC_FX_NONE ? "--" : ec_fx_short_name(m);
}
static void sends_page(const ec_app_t *a)
{
    static const char *const N[4] = {"I1", "I2", "S1", "S2"}, *const BOX[4] = {"I 1", "I 2", "S 1", "S 2"};
    const int post = opt(a, EC_P_SEND_TAP) == 2, y = 12, yb = 32, up = ec_hm_raised();
    const int tx = post ? 98 : 18, x0 = post ? 38 : tx, x1 = post ? tx + 2 : 80;
    char b[16];
    int i;
    /* the chain, left to right; MIX down to MST */
    block(2, 16, y, EC_C_L5, "FILT");
    block(82, 16, y, EC_C_L5, "AMP");
    block(102, 16, y, EC_C_L5, "MIX");
    for (i = 0; i < 4; i++)
        R(EC_C_L8, 18 + i * 30 + (i == 3 ? -10 : 0), y + 4, 4, 2);
    R(EC_C_L8, 108, y + 10, 2, yb - (y + 10));
    /* the sends on to MST */
    block(102, 16, yb, EC_C_L5, "MST");
    R(EC_C_L8, 56, yb + 4, 6, 2), R(EC_C_L8, 96, yb + 4, 6, 2);
    /* the tap (the send mode): down from the chain, along, down into S 1 and S 2 */
    R(EC_C_ACC, tx, y + 4, 4, 2);
    R(EC_C_ACC, tx + 1 - (tx & 1), y + 6, 2, 8);
    R(EC_C_ACC, x0 < x1 ? x0 : x1, y + 12, (x0 < x1 ? x1 - x0 : x0 - x1) + 2, 2);
    R(EC_C_ACC, 38, y + 12, 2, yb - (y + 12)), R(EC_C_ACC, 78, y + 12, 2, yb - (y + 12));
    /* the four slots last, so a raised one sits on top */
    slot_block(22, 26, y, EC_C_L12, BOX[0], up == 0);
    slot_block(52, 26, y, EC_C_L12, BOX[1], up == 1);
    slot_block(22, 34, yb, EC_C_L8, BOX[2], up == 2);
    slot_block(62, 34, yb, EC_C_L8, BOX[3], up == 3);
    T(EC_C_L12, post ? "POST" : "PRE", 2, yb + 3, EC_AL_LEFT);
    /* K1-K4's levels, each with its effect's name */
    for (i = 0; i < 4; i++) {
        const int x = 2 + (i % 2) * 60, yy = 46 + (i / 2) * 14;
        const float lv = clampf(slot_level(a, i) / 127.0f, 0, 1);
        snprintf(b, sizeof b, "%s %s", N[i], mach_name(a, i));
        T(EC_C_L8, b, x, yy, EC_AL_LEFT);
        R(EC_C_L3, x, yy + 6, 54, 4);
        R(lv > 0 ? EC_C_L12 : EC_C_L3, x, yy + 6, (int)(lv * 27.0f + 0.5f) * 2, 4);
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

/* 0.6.1: the held source key's EC_MS_* (0 none) */
static int held_src(const ec_app_t *a) { return a->mk.held >= 0 ? ec_mk_src_of_key(a->mk.held) : 0; }
/* 0.6.1: a depth -100..100 as 2-unit lattice cells 0..half; a non-zero depth at least one */
static int depth_cells(float d, int half)
{
    const int n = (int)(fabsf(d) / 100.0f * (float)half + 0.5f);
    return n > half ? half : n == 0 && d != 0 ? 1 : n;
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
                R(EC_C_ACC, mx, mid - h - 1, 2, h + 1);
            else if (h < 0)
                R(EC_C_ACC, mx, mid, 2, -h + 1);
            R(EC_C_L8, mx - 2, mid - 1, 6, 2);         /* the centre: 2 units (0.6.1) */
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
        R(EC_C_L8, VX0, VT - 6, 2, VB - VT + 6);
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
        R(EC_C_L12, x, v >= 0 ? (int)mid - (h > 2 ? h : 2) : (int)mid, 8, h > 2 ? h : 2);   /* 0.6.1: >= 2 units */
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
            R(EC_C_ACC, x - 2, VB - (int)(st.macro[k] * (VB - 16)), 18, 2);
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
        R(EC_C_ACC, 2, VB - (int)(st.macro[single] * (VB - 16)), 14, 2);
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
            /* 0.6.1: on a 2-unit lattice (18 cells from x 80, the centre tick cell 9, 8 each side), a tiny depth one cell; the
             * held source's bars in SRC */
            R(EC_C_L3, 80, y + 2, 36, 3);
            w = depth_cells(d, 8);
            if (w)
                R(held_src(a) == EC_MS_M1 + single ? EC_C_SRC : EC_C_L15, d >= 0 ? 100 : 98 - 2 * w, y + 2, 2 * w, 3);
            R(EC_C_L8, 98, y + 1, 2, 5);
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
        if (sel)                                    /* 0.6.1: 2 units round the text (over the gap below) */
            R(EC_C_L3, 0, y, W, 9);
        snprintf(n, sizeof n, "%d", k + 1);
        T(sel ? EC_C_L15 : EC_C_L5, n, 2, y + 2, EC_AL_LEFT);
        T(c, src ? ec_mtx_src_name(src) : "--", 9, y + 2, EC_AL_LEFT);
        ec_mk_dst_name(v[ec_mtx_dst_id(k)], nm, sizeof nm);
        T(c, ec_mtx_dst(v, k) >= 0 ? nm : "--", 29, y + 2, EC_AL_LEFT);
        ec_param_format(ec_mtx_depth_id(k), d, 3, dp, sizeof dp);
        T(c, dp, 78, y + 2, EC_AL_RIGHT);
        /* 0.6.1: on a 2-unit lattice (17 cells from x 84, the centre tick cell 8), a tiny depth one cell; the held
         * source's bars in SRC */
        R(EC_C_L5, 84, y + 3, 34, 2);
        w = depth_cells(d, 8);
        if (w)
            R(used && src == held_src(a) ? EC_C_SRC : sel ? EC_C_L15 : used ? EC_C_L12 : EC_C_L5,
              d >= 0 ? 102 : 100 - 2 * w, y + 3, 2 * w, 2);
        R(EC_C_L12, 100, y + 2, 2, 5);
        if (live) {                                 /* the amount now: source x depth */
            float x = 0;
            if (src >= EC_MS_LFO1 && src <= EC_MS_EXPR)
                x = st.src[src - EC_MS_LFO1];
            else if (src >= EC_MS_M1)
                x = st.macro[src - EC_MS_M1];
            x = clampf(x * d / 100.0f, -1, 1);
            R(EC_C_ACC, 84 + 2 * (8 + (int)lrintf(x * 8.0f)), y + 2, 2, 5);
        }
    }
}

void ec_scr_e4(const ec_app_t *a, const ec_page_t *pg)
{
    switch (pg->pic) {
    case EC_PIC_FX:                                 /* 0.6.3: EFFECTS (the slot shown), MASTER */
        if (pg->busv == 3)
            master_page(a);
        else
            effects_page(a, pg->busv ? pg->busv + 1 : a == &ec_app && ec_src_insert_page() ? ec_fx_slot() : 0);
        break;
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
