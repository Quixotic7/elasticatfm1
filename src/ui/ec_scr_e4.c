/* SPDX-License-Identifier: GPL-3.0-only */
/* E4 screens: the visual above the cells on the FX pages (per machine: delay taps, reverb decay, dynamics with the
 * live gain reduction, crushed waves, sweeps, EQ bands), SENDS (levels + the tap point), and MOD (LFO shapes with the
 * live value, the mod envelope, velocity, the macros and their matrix). Mockup grid 120 x 120 at EC_U. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_gfx.h"
#include "ec_mtx.h"
#include "ec_scr_e4.h"

#define W EC_GW
#define R(c, x, y, w, h) ec_gfx_rect(EC_U * (x), EC_U * (y), EC_U * (w), EC_U * (h), (c))
#define T(c, s, x, y, al) ec_gfx_text_al(EC_U * (x), EC_U * (y), (s), EC_U, (c), (al))
#define T2(c, s, x, y, al) ec_gfx_text_al(EC_U * (x), EC_U * (y), (s), 2 * EC_U, (c), (al))
#define LINE(c, x0, y0, x1, y1) ec_gfx_line((int)((x0) * EC_U), (int)((y0) * EC_U), (int)((x1) * EC_U), \
                                            (int)((y1) * EC_U), (c))

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
static void trace(const float *y, int n, float x0, float x1, uint16_t c, int thick)
{
    int i;
    for (i = 0; i + 1 < n; i++) {
        const float xa = x0 + (x1 - x0) * i / (n - 1), xb = x0 + (x1 - x0) * (i + 1) / (n - 1);
        LINE(c, xa, y[i], xb, y[i + 1]);
        if (thick)
            ec_gfx_line((int)(xa * EC_U), (int)(y[i] * EC_U) + 1, (int)(xb * EC_U), (int)(y[i + 1] * EC_U) + 1, c);
    }
}

static const float BEATS[16] = {0.125f, 1.f / 6, 0.25f, 1.f / 3, 0.375f, 0.5f, 2.f / 3, 0.75f,
                                1.f,    4.f / 3, 1.5f, 2.f,     3.f,    4.f,  8.f,     16.f};

/* ------------------------------------------------------------------------------------------- FX visuals --- */
#define VX0 3
#define VX1 108
#define VT 34
#define VB 70

static void vis_delay(const ec_app_t *a, int m)
{
    const int tid = m == EC_FX_DELAY ? EC_P_DELAY_TIME : EC_P_ECHO_TIME;
    const float tb = BEATS[(opt(a, tid) < 1 ? 1 : opt(a, tid) > 15 ? 15 : opt(a, tid)) - 1];
    const float fb = clampf(amt(a, EC_P_DELAY_FEEDBACK), 0, 0.95f);
    const float tone = m == EC_FX_DELAY ? amt(a, EC_P_DELAY_TONE) : amt(a, EC_P_ECHO_TONE);
    const float span = 8.0f, px = (VX1 - VX0) / span, mid = (VT + VB) / 2.0f, hh = (VB - VT) / 2.0f;
    const int echo = m == EC_FX_ECHO;
    const float off = echo ? powf(2.0f, bip(a, EC_P_ECHO_OFFSET)) : 1.0f;   /* linexp -1..1 -> 0.5..2 */
    const int ping = echo && opt(a, EC_P_ECHO_MODE) == 2;
    int b, k;
    for (b = 0; b <= 8; b++)
        R(b % 4 == 0 ? EC_C_L5 : EC_C_L3, VX0 + (int)(b * px), VB + 1, 1, 2);
    R(EC_C_L15, VX0, (int)(mid - hh), 2, (int)(2 * hh));
    for (k = 1; k < 64; k++) {
        const float g = powf(fb, (float)(k - 1)), h = hh * g;
        const uint16_t c = tone > 0.5f ? EC_C_L12 : EC_C_L8;
        const float xl = VX0 + k * tb * px, xr = VX0 + k * tb * off * px;
        if (h < 0.5f || (xl > VX1 && xr > VX1))
            break;
        if (!echo) {
            if (xl <= VX1)
                R(k == 1 ? EC_C_ACC : c, (int)xl, (int)(mid - h), 2, (int)(2 * h) < 1 ? 1 : (int)(2 * h));
        } else {                                /* L taps up, R taps down; ping-pong alternates the sides */
            const int up = !ping || (k & 1);
            if (xl <= VX1)
                R(k == 1 ? EC_C_ACC : c, (int)xl, up ? (int)(mid - h) : (int)mid, 2, (int)h < 1 ? 1 : (int)h);
            if (xr <= VX1)
                R(c, (int)xr, up ? (int)mid : (int)(mid - h), 2, (int)h < 1 ? 1 : (int)h);
        }
    }
    if (m == EC_FX_TAPE_ECHO) {                 /* the wobble: a wavy rule */
        float y[40];
        int i;
        const float w = amt(a, EC_P_TAPE_WOW), f = amt(a, EC_P_TAPE_FLUTTER);
        for (i = 0; i < 40; i++)
            y[i] = VT - 2 + 2.0f * w * sinf(i * 0.35f) + f * sinf(i * 2.1f);
        trace(y, 40, VX0, VX1, EC_C_L5, 0);
    }
}

static void vis_reverb(const ec_app_t *a, int m)
{
    const float size = amt(a, EC_P_REVERB_SIZE), damp = amt(a, EC_P_REVERB_DAMP);
    float tau, pre = 0;
    int c, freeze = 0;
    if (m == EC_FX_BLACKHOLE) {
        const float g = amt(a, EC_P_BH_GRAVITY);
        freeze = g > 0.995f;
        tau = 0.15f + 0.85f * (0.5f + size) * g * g;
        pre = amt(a, EC_P_BH_PREDELAY) * 0.15f;
    } else
        tau = 0.08f + 0.5f * size;
    for (c = 0; c <= VX1 - VX0; c++) {
        const float x = (float)c / (VX1 - VX0), t = x - pre;
        float e, eh;
        int h, hh2;
        if (t < 0)
            continue;
        e = freeze ? 0.8f : expf(-t / tau) * (t < 0.02f ? t / 0.02f : 1.0f);
        eh = e * expf(-t * damp * 6.0f);       /* the damped highs */
        h = (int)(e * (VB - VT));
        hh2 = (int)(eh * (VB - VT));
        if (h > 0)
            R((c & 1) ? EC_C_L5 : EC_C_L3, VX0 + c, VB - h, 1, h);
        if (hh2 > 0)
            R(EC_C_L12, VX0 + c, VB - hh2, 1, 1);
    }
    R(EC_C_ACC, VX0 + (int)(pre * (VX1 - VX0)), VT, 1, VB - VT);
}

/* COMP / LIMIT / DUCK: the static curve (comp / limit) + the live input and gain reduction */
static void vis_dyn(const ec_app_t *a, int m, const ec_fx_meter_t *mt)
{
    const float x0 = VX0, s = (float)(VB - VT);
    float thr_db = -99, slope = 1, gain_db = 0;
    int c;
    if (m == EC_FX_COMP) {
        thr_db = 20.0f * log10f(0.02f + amt(a, EC_P_COMP_THRESH) * 0.98f);
        {
            const float r = amt(a, EC_P_COMP_RATIO);
            slope = r <= 0.001f ? 1.0f : powf(0.05f, (r - 0.001f) / 0.999f);
        }
    } else if (m == EC_FX_LIMIT) {
        thr_db = -12.0f + 12.0f * amt(a, EC_P_LIMIT_CEIL);
        slope = 0;
        gain_db = 18.0f * amt(a, EC_P_LIMIT_GAIN);
    }
    if (m != EC_FX_DUCK) {                      /* transfer: in -48..0 dB across, out -48..0 dB up */
        R(EC_C_L3, (int)x0, VT, 1, (int)s);
        R(EC_C_L3, (int)x0, VB, (int)s, 1);
        for (c = 0; c < (int)s; c++) {
            const float in = -48.0f + 48.0f * c / s + gain_db;
            float out = in > thr_db ? thr_db + (in - thr_db) * slope : in;
            int y;
            out = clampf(out, -48, 0);
            y = VB - (int)((out + 48.0f) / 48.0f * s);
            R(EC_C_L12, (int)x0 + c, y, 1, 1);
        }
        if (thr_db > -48)
            R(EC_C_ACC, (int)x0 + (int)((thr_db - gain_db + 48.0f) / 48.0f * s), VT, 1, (int)s);
        if (mt->valid) {                       /* the live input on the curve */
            const float in = clampf(mt->key_db, -48, 0);
            R(EC_C_L15, (int)x0 + (int)((in + 48.0f) / 48.0f * s) - 1, VB - 3, 2, 3);
        }
    }
    {   /* meters: IN (key) and GR (down from the top, accent) */
        const int mx = m == EC_FX_DUCK ? 20 : 54;
        const float in = mt->valid ? clampf((mt->key_db + 48.0f) / 48.0f, 0, 1) : 0;
        const float gr = mt->valid ? clampf(-mt->gr_db / 24.0f, 0, 1) : 0;
        int k;
        T(EC_C_L8, "IN", mx, VT, EC_AL_LEFT);
        R(EC_C_L3, mx + 12, VT + 1, 40, 4);
        R(EC_C_L12, mx + 12, VT + 1, (int)(in * 40), 4);
        T(EC_C_L8, "GR", mx, VT + 12, EC_AL_LEFT);
        R(EC_C_L3, mx + 12, VT + 13, 40, 4);
        R(EC_C_ACC, mx + 52 - (int)(gr * 40), VT + 13, (int)(gr * 40), 4);
        for (k = 0; k <= 4; k++)
            R(EC_C_L5, mx + 12 + k * 10, VT + 18, 1, 1);
        if (mt->valid) {
            char b[16];
            snprintf(b, sizeof b, "%.1fDB", (double)mt->gr_db);
            T2(gr > 0.02f ? EC_C_ACC : EC_C_L8, b, mx + 52, VT + 24, EC_AL_RIGHT);
        }
    }
}

/* LOFI / DESTROY: a sine through the hold + the quantizer (+ fold / saturation) */
static void vis_crush(const ec_app_t *a, int m)
{
    float y[106];
    const int n = 106;
    float holds, levels, drive = 0, fold = 0, sat = 0;
    int i;
    if (m == EC_FX_LOFI) {
        const float hz = 1000.0f * powf(48.0f, val(a, EC_P_LOFI_RATE) / 127.0f);
        const float bits = 1.0f + val(a, EC_P_LOFI_BITS) / 127.0f * 23.0f;
        holds = clampf(hz / 400.0f, 3, 106);
        levels = powf(2.0f, clampf(bits - 1.0f, 0, 6));
    } else {
        const float crush = amt(a, EC_P_DESTROY_CRUSH), srr = amt(a, EC_P_DESTROY_SRR);
        holds = clampf(106.0f * powf(1.0f / 30.0f, srr), 3, 106);
        levels = powf(2.0f, clampf(5.0f * (1.0f - crush) + 1.0f, 1, 6));
        drive = amt(a, EC_P_FX_DRIVE), fold = amt(a, EC_P_DESTROY_FOLD), sat = amt(a, EC_P_DESTROY_SAT);
    }
    for (i = 0; i < n; i++) {
        const float t = floorf((float)i * holds / n) / holds;
        float x = 0.9f * sinf(6.2831853f * 2.0f * t);
        if (drive > 0)
            x = tanhf(x * (1 + drive * 24));
        if (fold > 0) {
            x *= 1 + fold * 6;
            while (x > 1 || x < -1)
                x = x > 1 ? 2 - x : -2 - x;
        }
        if (sat > 0)
            x = tanhf(x * (1 + sat * 14));
        x = floorf(x * levels * 0.5f + 0.5f) / (levels * 0.5f);
        y[i] = (VT + VB) / 2.0f - clampf(x, -1, 1) * (VB - VT) / 2.0f;
    }
    ec_gfx_dots_h(EC_U * VX0, EC_U * VX1, EC_U * (VT + VB) / 2, 2 * EC_U, EC_U, EC_C_L3);
    for (i = 0; i + 1 < n; i++) {
        R(EC_C_L12, VX0 + i, (int)y[i], 1, 1);
        if (fabsf(y[i + 1] - y[i]) >= 1)
            LINE(EC_C_L8, VX0 + i + 1, y[i], VX0 + i + 1, y[i + 1]);
    }
}

/* CHORUS / FLANGER / PHASER / RINGS / CASSETTE / MOTION: a sweep */
static void vis_sweep(const ec_app_t *a, int m)
{
    float y[106], y2[106];
    const int n = 106;
    const float mid = (VT + VB) / 2.0f, hh = (VB - VT) / 2.0f;
    int i, two = 0;
    for (i = 0; i < n; i++) {
        const float x = (float)i / (n - 1);
        float v = 0, v2 = 0;
        switch (m) {
        case EC_FX_CHORUS:
        case EC_FX_FLANGER:
        case EC_FX_PHASER: {
            const float cyc = 1 + 5 * amt(a, EC_P_MODFX_RATE), d = 0.15f + 0.85f * amt(a, EC_P_MODFX_DEPTH);
            v = d * sinf(6.2831853f * cyc * x);
            if (m == EC_FX_CHORUS) {
                v2 = d * sinf(6.2831853f * cyc * x + amt(a, EC_P_CHORUS_WIDTH) * 3.14159f);
                two = 1;
            }
        } break;
        case EC_FX_RINGS: {
            const float f = fabsf(bip(a, EC_P_RINGS_FREQ));
            v = sinf(6.2831853f * 2 * x) * sinf(6.2831853f * (3 + 30 * f) * x) * 0.9f;
        } break;
        case EC_FX_CASSETTE: {
            const float w = amt(a, EC_P_TAPE_WOW), f = amt(a, EC_P_TAPE_FLUTTER);
            v = 0.7f * sinf(6.2831853f * 3 * x + w * 1.5f * sinf(6.2831853f * x) + f * 0.6f * sinf(80 * x));
        } break;
        default: {                               /* MOTION: L / R gains over two cycles */
            const int sh = opt(a, EC_P_MOTION_SHAPE);
            const float ph = 2 * x - floorf(2 * x);
            const float lfo = sh == 3 ? (ph < 0.5f ? 1.f : 0.f)
                              : sh == 2 ? 1 - fabsf(2 * ph - 1) : 0.5f + 0.5f * sinf(6.2831853f * ph);
            const float tr = 1 - amt(a, EC_P_MOTION_TREM) * lfo, pn = amt(a, EC_P_MOTION_PAN);
            v = tr * (1 - pn * lfo) * 2 - 1;
            v2 = tr * (1 - pn * (1 - lfo)) * 2 - 1;
            two = 1;
        } break;
        }
        y[i] = mid - v * hh;
        y2[i] = mid - v2 * hh;
    }
    ec_gfx_dots_h(EC_U * VX0, EC_U * VX1, EC_U * (int)mid, 2 * EC_U, EC_U, EC_C_L3);
    if (two)
        trace(y2, n, VX0, VX1, EC_C_L8, 0);
    trace(y, n, VX0, VX1, EC_C_L12, 1);
    if (m == EC_FX_PHASER) {                    /* the notches: one per two stages */
        const int st = opt(a, EC_P_PHASER_STAGES), cn = amt(a, EC_P_PHASER_CENTER) * (VX1 - VX0);
        int k;
        for (k = 0; k < (st + 1) / 2; k++)
            R(EC_C_ACC, VX0 + (int)(cn * (k + 1) / ((st + 1) / 2 + 1) * 1.6f) % (VX1 - VX0), VB + 1, 2, 2);
    }
    if (m == EC_FX_CASSETTE && amt(a, EC_P_CASS_NOISE) > 0) {
        uint32_t r = 0x2468ACEu;
        int k, cnt = (int)(amt(a, EC_P_CASS_NOISE) * 60);
        for (k = 0; k < cnt; k++) {
            r = r * 1664525u + 1013904223u;
            R(EC_C_L5, VX0 + (int)(r >> 8) % (VX1 - VX0), VT + (int)(r >> 20) % (VB - VT), 1, 1);
        }
    }
}

static void vis_eq(const ec_app_t *a)
{
    const float xl = 0.15f + 0.25f * amt(a, EC_P_EQ_XLOW), xh = 0.55f + 0.3f * amt(a, EC_P_EQ_XHI);
    const float g[3] = {1 + bip(a, EC_P_EQ_LOW), 1 + bip(a, EC_P_EQ_MID), 1 + bip(a, EC_P_EQ_HIGH)};
    const float e[4] = {0, xl, xh, 1};
    const float unity = VB - (VB - VT) / 2.0f;
    int b;
    for (b = 0; b < 3; b++) {
        const int x0 = VX0 + (int)(e[b] * (VX1 - VX0)) + 1, x1 = VX0 + (int)(e[b + 1] * (VX1 - VX0)) - 1;
        const int h = (int)(g[b] / 2.0f * (VB - VT));
        R(g[b] < 0.02f ? EC_C_ACC : EC_C_L5, x0, VB - (h > 0 ? h : 1), x1 - x0, h > 0 ? h : 1);
        R(EC_C_L12, x0, VB - h, x1 - x0, 1);
    }
    ec_gfx_dots_h(EC_U * VX0, EC_U * VX1, EC_U * (int)unity, 2 * EC_U, EC_U, EC_C_L8);
}

static void fx_visual(const ec_app_t *a, int m, const ec_fx_meter_t *mt)
{
    switch (m) {
    case EC_FX_DELAY:
    case EC_FX_ECHO:
    case EC_FX_TAPE_ECHO: vis_delay(a, m); break;
    case EC_FX_REVERB:
    case EC_FX_BLACKHOLE: vis_reverb(a, m); break;
    case EC_FX_COMP:
    case EC_FX_LIMIT:
    case EC_FX_DUCK: vis_dyn(a, m, mt); break;
    case EC_FX_LOFI:
    case EC_FX_DESTROY: vis_crush(a, m); break;
    case EC_FX_DJ_EQ: vis_eq(a); break;
    case EC_FX_NONE:
        ec_gfx_dots_h(EC_U * VX0, EC_U * VX1, EC_U * (VT + VB) / 2, 3 * EC_U, EC_U, EC_C_L5);
        break;
    default: vis_sweep(a, m); break;
    }
}

static void level_bar(float pk, int x, int top, int bot)
{
    const int h = (int)(clampf(pk, 0, 1) * (bot - top));
    R(EC_C_L3, x, top, 2, bot - top);
    R(pk > 0.89f ? EC_C_ACC : EC_C_L12, x, bot - h, 2, h);
}

static void fx_page(const ec_app_t *a, const ec_page_t *pg)
{
    static const char *const SLOT[4] = {"INSERT", "SEND 1", "SEND 2", "MASTER"};
    const int m = opt(a, EC_P_FX_MACHINE);
    ec_fx_meter_t mt;
    float pk;
    int running;
    memset(&mt, 0, sizeof mt);
    if (pg->busv) {
        ec_bus_status_t bs;
        ec_dsp_bus_status(a->dsp, pg->busv - 1, &bs);
        mt = bs.fx_meter;
        pk = bs.peak_l > bs.peak_r ? bs.peak_l : bs.peak_r;
        running = bs.fx_running;
    } else {
        ec_track_status_t st;
        ec_dsp_track_status(a->dsp, a->track, &st);
        mt = st.fx_meter;
        pk = st.peak_l > st.peak_r ? st.peak_l : st.peak_r;
        running = st.fx_running;
    }
    T(EC_C_L8, SLOT[pg->busv], 2, 13, EC_AL_LEFT);
    T2(m == EC_FX_NONE ? EC_C_L5 : EC_C_L15, ec_fx_name(m), 2, 20, EC_AL_LEFT);
    if (running != m)                           /* the swap fade in progress */
        R(EC_C_ACC, 2, 31, 6, 1);
    fx_visual(a, m, &mt);
    level_bar(pk, W - 6, VT, VB);
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
static void routes(const ec_app_t *a, int src)
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
    routes(a, k ? EC_MS_LFO2 : EC_MS_LFO1);
    for (i = 0; i < 104; i++) {
        const float x = (float)i / 103 * cyc, ph = x - floorf(x);
        const float w = mode == 3 && x >= 1 ? wave_at(wave, 1.0f) : wave_at(wave, mode == 3 ? x : ph);
        y[i] = mid - w * d * hh;
    }
    ec_gfx_dots_h(EC_U * VX0, EC_U * VX1, EC_U * (int)mid, 2 * EC_U, EC_U, EC_C_L3);
    if (mode == 2 || mode == 3)                 /* TRIG / ONE: the reset at the note */
        R(EC_C_L8, VX0, VT - 6, 1, VB - VT + 6);
    trace(y, 104, VX0, VX1, d == 0 ? EC_C_L5 : EC_C_L12, 1);
}

static void menv_page(const ec_app_t *a)
{
    const int er = opt(a, EC_P_ENV_RANGE);
    const float at = ec_xf_env_seconds(val(a, EC_P_MENV_ATTACK), er), dc = ec_xf_env_seconds(val(a, EC_P_MENV_DECAY), er);
    const float rl = ec_xf_env_seconds(val(a, EC_P_MENV_RELEASE), er), su = amt(a, EC_P_MENV_SUSTAIN);
    const float d = bip(a, EC_P_MENV_DEPTH);
    const float base = d >= 0 ? VB : VT, peak = d >= 0 ? VB - (VB - VT) * fabsf(d) : VT + (VB - VT) * fabsf(d);
    float xs[5], ys[5], x = VX0;
    int i;
#define EW(s) (3.0f + 7.0f * log2f(1.0f + (s) * 20.0f) > 30 ? 30 : 3.0f + 7.0f * log2f(1.0f + (s) * 20.0f))
    xs[0] = x, ys[0] = base;
    x += EW(at), xs[1] = x, ys[1] = peak;
    x += EW(dc), xs[2] = x, ys[2] = base + (peak - base) * su;
    x += 18, xs[3] = x, ys[3] = ys[2];
    x += EW(rl), xs[4] = x, ys[4] = base;
#undef EW
    ec_gfx_dots_h(EC_U * VX0, EC_U * VX1, EC_U * (int)base, 2 * EC_U, EC_U, EC_C_L3);
    for (i = 0; i < 4; i++) {
        LINE(d == 0 ? EC_C_L5 : EC_C_L12, xs[i], ys[i], xs[i + 1], ys[i + 1]);
        R(EC_C_L15, (int)xs[i + 1] - 1, (int)ys[i + 1] - 1, 2, 2);
    }
    routes(a, EC_MS_ENV);
}

static void vel_page(const ec_app_t *a)
{
    const float d = bip(a, EC_P_MVEL_DEPTH), mid = (VT + VB) / 2.0f, hh = (VB - VT) / 2.0f;
    int k;
    routes(a, EC_MS_VEL);
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
    for (k = 0; k < EC_MTX_SLOTS; k++) {
        const int y = 14 + k * 7, sel = k == a->mk.row, src = ec_mtx_src(v, k), live = ec_mtx_live(v, k);
        const float d = v[ec_mtx_depth_id(k)];
        const uint16_t c = sel ? EC_C_L15 : live ? EC_C_L8 : EC_C_L5;
        char n[4], nm[16], dp[12];
        int w;
        if (sel)
            R(EC_C_L3, 0, y - 1, W, 7);
        snprintf(n, sizeof n, "%d", k + 1);
        T(sel ? EC_C_L15 : EC_C_L5, n, 2, y + 1, EC_AL_LEFT);
        T(c, src ? ec_mtx_src_name(src) : "--", 8, y + 1, EC_AL_LEFT);
        ec_mk_dst_name(v[ec_mtx_dst_id(k)], nm, sizeof nm);
        T(c, ec_mtx_dst(v, k) >= 0 ? nm : "--", 28, y + 1, EC_AL_LEFT);
        ec_param_format(ec_mtx_depth_id(k), d, 3, dp, sizeof dp);
        T(c, dp, 80, y + 1, EC_AL_RIGHT);
        R(EC_C_L5, 84, y + 3, 34, 1);
        w = (int)(fabsf(d) / 100.0f * 17.0f + 0.5f);
        if (w)
            R(sel ? EC_C_L15 : EC_C_L8, d >= 0 ? 101 : 101 - w, y + 2, w, 3);
        R(sel ? EC_C_L15 : EC_C_L8, 101, y + 1, 1, 5);
        if (live) {                                 /* the amount now: source x depth */
            float x = 0;
            if (src >= EC_MS_LFO1 && src <= EC_MS_EXPR)
                x = st.src[src - EC_MS_LFO1];
            else if (src >= EC_MS_M1)
                x = st.macro[src - EC_MS_M1];
            x = clampf(x * d / 100.0f, -1, 1);
            R(EC_C_ACC, 101 + (int)(x * 17.0f), y + 1, 1, 5);
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
